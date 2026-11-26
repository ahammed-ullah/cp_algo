#include "cp_algo/weights.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>

using namespace std;

namespace cp_algo {

vector<uint64_t> generated_weights(int n) {
    const uint64_t bound = static_cast<uint64_t>(n) * n;
    vector<uint64_t> weights;
    weights.reserve(static_cast<size_t>(n));
    mt19937_64 generator(default_weight_seed);
    if (n == 0) return weights;

    // Reject the short prefix of the 64-bit range so modulo has no bias. The
    // engine, seed, rejection rule and vertex order completely specify weights.
    const uint64_t threshold = (uint64_t(0) - bound) % bound;
    for (int vertex = 0; vertex < n; ++vertex) {
        uint64_t draw;
        do { draw = generator(); } while (draw < threshold);
        weights.push_back(1 + draw % bound);
    }
    return weights;
}

vector<uint64_t> load_weights(const filesystem::path& path, int n) {
    ifstream file(path);
    if (!file) throw runtime_error("cannot open weights file: " + path.string());
    vector<uint64_t> weights;
    weights.reserve(static_cast<size_t>(n));
    string line;
    size_t line_number = 0;
    while (getline(file, line)) {
        ++line_number;
        const auto comment = line.find('#');
        if (comment != string::npos) line.resize(comment);
        const auto first = line.find_first_not_of(" \t\r\n");
        if (first == string::npos) continue;
        const auto last = line.find_last_not_of(" \t\r\n");
        const auto token = line.substr(first, last - first + 1);
        if (token.empty() || !all_of(token.begin(), token.end(), [](unsigned char c) {
            return isdigit(c) != 0;
        })) throw runtime_error("invalid weight at line " + to_string(line_number));
        size_t consumed = 0;
        uint64_t weight = 0;
        try { weight = stoull(token, &consumed); }
        catch (const exception&) {
            throw runtime_error("weight outside uint64 range at line " +
                                     to_string(line_number));
        }
        if (consumed != token.size() || weight == 0)
            throw runtime_error("weight must be a positive integer at line " +
                                     to_string(line_number));
        weights.push_back(weight);
        if (weights.size() > static_cast<size_t>(n))
            throw runtime_error("weights file has more than n values");
    }
    if (weights.size() != static_cast<size_t>(n))
        throw runtime_error("weights file needs exactly n values");
    return weights;
}

string decimal(__uint128_t value) {
    if (value == 0) return "0";
    string result;
    while (value != 0) {
        result.push_back(static_cast<char>('0' + value % 10));
        value /= 10;
    }
    reverse(result.begin(), result.end());
    return result;
}

}  // namespace cp_algo
