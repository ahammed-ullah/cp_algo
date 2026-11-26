#include "cp_algo/ccf.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

using namespace std;

namespace cp_algo {
namespace {

constexpr size_t kHeaderBytes = 160;
constexpr array<uint8_t, 8> kCcfMagic = {
    'C', 'C', 'F', 0, '\r', '\n', 0x1a, '\n'};

uint64_t little_endian(const vector<uint8_t>& bytes,
                       size_t at, unsigned count) {
    uint64_t value = 0;
    for (unsigned i = 0; i < count; ++i) {
        value |= static_cast<uint64_t>(bytes[at + i]) << (8 * i);
    }
    return value;
}

string hex_digest(const vector<uint8_t>& bytes, size_t at) {
    ostringstream out;
    out << hex << setfill('0');
    for (size_t i = 0; i < 32; ++i) out << setw(2) << unsigned(bytes[at + i]);
    return out.str();
}

[[noreturn]] void malformed(const string& detail) {
    throw runtime_error("invalid CCF: " + detail);
}

}  // namespace

Ccf Ccf::load(const filesystem::path& input_path) {
    ifstream file(input_path, ios::binary | ios::ate);
    if (!file) throw runtime_error("cannot open CCF: " + input_path.string());
    const auto length = file.tellg();
    if (length < static_cast<streamoff>(kHeaderBytes)) malformed("short header");

    Ccf ccf;
    ccf.path = input_path;
    ccf.bytes_.resize(static_cast<size_t>(length));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(ccf.bytes_.data()), length);
    if (!file) throw runtime_error("cannot read CCF: " + input_path.string());

    const auto& bytes = ccf.bytes_;
    if (!equal(kCcfMagic.begin(), kCcfMagic.end(), bytes.begin())) malformed("magic");
    if (little_endian(bytes, 8, 4) != 1 ||
        little_endian(bytes, 12, 4) != kHeaderBytes ||
        little_endian(bytes, 16, 4) != 1 ||
        little_endian(bytes, 20, 4) != 0) malformed("header version or encoding");

    const auto n = little_endian(bytes, 24, 8);
    const auto p = little_endian(bytes, 40, 8);
    if (n > INT_MAX || p > INT_MAX) malformed("labels exceed cc_con's int range");
    ccf.n = static_cast<int>(n);
    ccf.m = little_endian(bytes, 32, 8);
    ccf.num_cliques = static_cast<int>(p);
    ccf.cover_size = little_endian(bytes, 48, 8);
    ccf.file_bytes = little_endian(bytes, 88, 8);
    ccf.graph_fingerprint_sha256 = hex_digest(bytes, 96);
    ccf.file_checksum_sha256 = hex_digest(bytes, 128);

    const auto directory = little_endian(bytes, 56, 8);
    const auto entries = little_endian(bytes, 64, 8);
    const auto payload = little_endian(bytes, 72, 8);
    const auto payload_bytes = little_endian(bytes, 80, 8);
    if (directory != kHeaderBytes || entries != p + 1 ||
        payload != kHeaderBytes + 8 * entries ||
        payload > bytes.size() || payload_bytes != bytes.size() - payload ||
        ccf.file_bytes != bytes.size()) malformed("directory or file length");

    // Check the offset table needed for safe record access. cc_check performs
    // the full record and checksum validation separately, before experiment
    // runs; decoding it here would add an unused second cover scan.
    if (ccf.offset(0) != payload || ccf.offset(ccf.num_cliques) != bytes.size())
        malformed("first or final record offset");
    for (int label = 0; label < ccf.num_cliques; ++label) {
        const auto start = ccf.offset(label);
        const auto end = ccf.offset(label + 1);
        if (start > end || start < payload || end > bytes.size())
            malformed("record offset order");
    }
    return ccf;
}

uint64_t Ccf::offset(int label) const {
    return little_endian(bytes_, kHeaderBytes + 8 * static_cast<size_t>(label), 8);
}

uint64_t Ccf::read_vbyte(uint64_t& position, uint64_t end) const {
    uint64_t value = 0;
    unsigned shift = 0;
    while (position < end && shift < 64) {
        const auto byte = bytes_[static_cast<size_t>(position++)];
        if (shift == 63 && (byte & 0x7eu) != 0)
            malformed("vbyte exceeds 64 bits");
        value |= static_cast<uint64_t>(byte & 0x7fu) << shift;
        if ((byte & 0x80u) == 0) {
            if (shift != 0 && (byte & 0x7fu) == 0)
                malformed("noncanonical vbyte");
            return value;
        }
        shift += 7;
    }
    malformed("truncated or overlong vbyte");
}

Ccf::CliqueCursor::CliqueCursor(const Ccf& ccf, int label)
    : ccf_(&ccf), position_(ccf.offset(label)), end_(ccf.offset(label + 1)),
      remaining_(ccf.read_vbyte(position_, end_)) {}

bool Ccf::CliqueCursor::next(int& vertex) {
    if (remaining_ == 0) return false;
    const auto part = ccf_->read_vbyte(position_, end_);
    vertex = first_ ? static_cast<int>(part) : previous_ + static_cast<int>(part);
    previous_ = vertex;
    first_ = false;
    --remaining_;
    return true;
}

Ccf::CompactCliqueCursor Ccf::compact_cursor(int label) const {
    auto position = offset(label);
    const auto end = offset(label + 1);
    read_vbyte(position, end);  // Skip the clique cardinality.
    return {static_cast<uint32_t>(position), -1};
}

bool Ccf::next_compact(int label, CompactCliqueCursor& scan, int& vertex) const {
    auto position = static_cast<uint64_t>(scan.position);
    const auto end = offset(label + 1);
    if (position == end) return false;
    const auto part = read_vbyte(position, end);
    vertex = scan.previous < 0 ? static_cast<int>(part)
                               : scan.previous + static_cast<int>(part);
    scan.position = static_cast<uint32_t>(position);
    scan.previous = vertex;
    return true;
}

void Ccf::decode_clique(int label, vector<int>& vertices) const {
    vertices.clear();
    auto scan = cursor(label);
    int vertex;
    while (scan.next(vertex)) vertices.push_back(vertex);
}

vector<vector<int>> Ccf::decode_all() const {
    vector<vector<int>> C(num_cliques);
    for (int ell = 0; ell < num_cliques; ++ell) decode_clique(ell, C[ell]);
    return C;
}

void Ccf::release_bytes() {
    vector<uint8_t>().swap(bytes_);
}

}  // namespace cp_algo
