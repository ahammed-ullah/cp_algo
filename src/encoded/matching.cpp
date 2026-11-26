#include "cp_algo/algorithms.hpp"
#include "cp_algo/vbyte.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace std;

namespace cp_algo {

namespace {

uint32_t vbyte_bytes(uint32_t value) {
    uint32_t count = 1;
    while (value >= 0x80u) {
        value >>= 7;
        ++count;
    }
    return count;
}

void write_vbyte(vector<uint8_t>& bytes, uint32_t& position, uint32_t value) {
    while (value >= 0x80u) {
        bytes[position++] = static_cast<uint8_t>((value & 0x7fu) | 0x80u);
        value >>= 7;
    }
    bytes[position++] = static_cast<uint8_t>(value);
}

}  // namespace

int matching_encoded(const Ccf& ccf, MatchingVariant variant) {
    vector<int> mate(ccf.n, -1);
    int matching_size = 0;

    if (variant == MatchingVariant::Maximal) {
        // Figure 5 can stream CCF cliques; it needs no saved cover or Gamma.
        for (int q = 0; q < ccf.num_cliques; ++q) {
            auto clique = ccf.cursor(q);
            int pending = -1;
            int v;
            while (clique.next(v)) {
                if (mate[v] != -1) continue;
                if (pending == -1) {
                    pending = v;
                } else {
                    mate[pending] = v;
                    mate[v] = pending;
                    ++matching_size;
                    pending = -1;
                }
            }
        }
        return matching_size;
    }

    // Each candidate uses at most five vbyte bytes. This one bound keeps
    // every list count within the 32-bit Gamma positions used below.
    if (ccf.cover_size > numeric_limits<uint32_t>::max() / 5)
        throw runtime_error("Gamma may exceed 32-bit byte offsets");

    // Figure 6: count the bytes of each Gamma[v] while forming M0's
    // initial matching. Each clique is read a second time only when it has
    // a remaining free vertex. No decoded clique is kept in memory.
    vector<uint32_t> gamma_end(ccf.n + 1, 0);
    vector<pair<int, int>> M0;
    for (int q = 0; q < ccf.num_cliques; ++q) {
        auto clique = ccf.cursor(q);
        int pending = -1;
        int v;
        while (clique.next(v)) {
            if (mate[v] != -1) continue;
            if (pending == -1) {
                pending = v;
            } else {
                mate[pending] = v;
                mate[v] = pending;
                ++matching_size;
                pending = -1;
            }
        }
        if (pending != -1) {
            auto members = ccf.cursor(q);
            const uint32_t width = vbyte_bytes(static_cast<uint32_t>(pending));
            while (members.next(v))
                if (v != pending) gamma_end[v] += width;
        }
    }

    // Convert byte counts to segment starts. During the next scan each
    // start advances as its list is filled. At the end it is that list's
    // end, so one array supplies both boundaries without write pointers.
    uint64_t bytes = 0;
    for (int v = 0; v < ccf.n; ++v) {
        const uint32_t count = gamma_end[v];
        gamma_end[v] = static_cast<uint32_t>(bytes);
        bytes += count;
    }
    if (bytes > numeric_limits<uint32_t>::max())
        throw runtime_error("Gamma exceeds 32-bit byte offsets");
    gamma_end[ccf.n] = static_cast<uint32_t>(bytes);
    vector<uint8_t> gamma(static_cast<size_t>(bytes));

    // Recreate the same initial matching and append candidates in clique
    // order. This second scan needs no saved per-clique pending vertices.
    M0.reserve(matching_size);
    fill(mate.begin(), mate.end(), -1);
    matching_size = 0;
    for (int q = 0; q < ccf.num_cliques; ++q) {
        auto clique = ccf.cursor(q);
        int pending = -1;
        int v;
        while (clique.next(v)) {
            if (mate[v] != -1) continue;
            if (pending == -1) {
                pending = v;
            } else {
                mate[pending] = v;
                mate[v] = pending;
                M0.emplace_back(pending, v);
                ++matching_size;
                pending = -1;
            }
        }
        if (pending != -1) {
            auto members = ccf.cursor(q);
            while (members.next(v))
                if (v != pending)
                    write_vbyte(gamma, gamma_end[v],
                                static_cast<uint32_t>(pending));
        }
    }

    // Figure 6: inspect the current free candidates for each edge of M0.
    for (const auto& [u, v] : M0) {
        if (mate[u] != v) continue;
        int x = -1, other_x = -1;
        int y = -1, other_y = -1;
        size_t pos = u == 0 ? 0 : gamma_end[u - 1];
        while (pos < gamma_end[u]) {
            int w = static_cast<int>(read_vbyte(gamma, pos));
            if (mate[w] != -1) continue;
            if (x == -1) x = w;
            else if (w != x) {
                other_x = w;
                break;
            }
        }
        pos = v == 0 ? 0 : gamma_end[v - 1];
        while (pos < gamma_end[v]) {
            int w = static_cast<int>(read_vbyte(gamma, pos));
            if (mate[w] != -1) continue;
            if (y == -1) y = w;
            else if (w != y) {
                other_y = w;
                break;
            }
        }
        if (x == -1 || y == -1) continue;
        if (x == y) {
            if (other_x != -1) x = other_x;
            else y = other_y;
        }
        if (y == -1) continue;

        mate[u] = x;
        mate[x] = u;
        mate[v] = y;
        mate[y] = v;
        ++matching_size;
    }
    return matching_size;
}

}  // namespace cp_algo
