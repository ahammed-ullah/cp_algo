#include "cp_algo/config.hpp"

#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

using namespace std;

namespace cp_algo {
namespace {

string trim(const string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

string without_comment(const string& text) {
    bool quoted = false;
    bool escaped = false;
    for (size_t i = 0; i < text.size(); ++i) {
        if (escaped) { escaped = false; continue; }
        if (quoted && text[i] == '\\') { escaped = true; continue; }
        if (text[i] == '"') quoted = !quoted;
        if (text[i] == '#' && !quoted) return text.substr(0, i);
    }
    return text;
}

string quoted_string(const string& raw, size_t line) {
    if (raw.size() < 2 || raw.front() != '"' || raw.back() != '"')
        throw runtime_error("config line " + to_string(line) +
                                 ": expected a quoted TOML string");
    string result;
    for (size_t i = 1; i + 1 < raw.size(); ++i) {
        if (raw[i] != '\\') { result.push_back(raw[i]); continue; }
        if (++i + 1 >= raw.size())
            throw runtime_error("config line " + to_string(line) +
                                     ": incomplete string escape");
        switch (raw[i]) {
            case '\\': result.push_back('\\'); break;
            case '"': result.push_back('"'); break;
            case 'n': result.push_back('\n'); break;
            case 't': result.push_back('\t'); break;
            default:
                throw runtime_error("config line " + to_string(line) +
                                         ": unsupported string escape");
        }
    }
    return result;
}

}  // namespace

Config load_config(const filesystem::path& path) {
    ifstream file(path);
    if (!file) throw runtime_error("cannot open config: " + path.string());
    Config config;
    config.source_path = filesystem::absolute(path).lexically_normal();
    unordered_set<string> seen;
    string line;
    size_t number = 0;
    while (getline(file, line)) {
        ++number;
        line = trim(without_comment(line));
        if (line.empty()) continue;
        const auto equal = line.find('=');
        if (equal == string::npos)
            throw runtime_error("config line " + to_string(number) +
                                     ": expected key = value");
        const auto key = trim(line.substr(0, equal));
        const auto raw_value = trim(line.substr(equal + 1));
        if (!seen.insert(key).second)
            throw runtime_error("duplicate config key: " + key);
        if (key == "mvm_epsilon") {
            size_t length = 0;
            try {
                config.mvm_epsilon = stod(raw_value, &length);
            } catch (const exception&) {
                throw runtime_error("mvm_epsilon must be a positive finite number");
            }
            if (length != raw_value.size() || !isfinite(config.mvm_epsilon) ||
                config.mvm_epsilon <= 0)
                throw runtime_error("mvm_epsilon must be a positive finite number");
            config.mvm_epsilon_specified = true;
            continue;
        }
        const auto value = quoted_string(raw_value, number);
        if (key == "cover_mode") {
            if (value == "plain") config.cover_mode = CoverMode::Plain;
            else if (value == "encoded") config.cover_mode = CoverMode::Encoded;
            else throw runtime_error("cover_mode must be plain or encoded");
        } else if (key == "matching_variant") {
            config.matching_variant_specified = true;
            if (value == "three_halves") config.matching_variant = MatchingVariant::ThreeHalves;
            else if (value == "maximal") config.matching_variant = MatchingVariant::Maximal;
            else throw runtime_error("matching_variant must be three_halves or maximal");
        } else if (key == "mvm_mode") {
            config.mvm_mode_specified = true;
            if (value == "bounded") config.mvm_mode = MvmMode::Bounded;
            else if (value == "until_no_improvement")
                config.mvm_mode = MvmMode::UntilNoImprovement;
            else throw runtime_error("mvm_mode must be bounded or until_no_improvement");
        } else if (key == "weights_file") {
            filesystem::path weight_path(value);
            if (weight_path.is_relative())
                weight_path = config.source_path->parent_path() / weight_path;
            config.weights_file = filesystem::absolute(weight_path).lexically_normal();
        } else {
            throw runtime_error("unknown config key: " + key);
        }
    }
    return config;
}

}  // namespace cp_algo
