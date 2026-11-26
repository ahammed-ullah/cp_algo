#include "cp_algo/algorithms.hpp"
#include "cp_algo/config.hpp"
#include "cp_algo/report.hpp"
#include "cp_algo/weights.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/resource.h>

using namespace std;

namespace {

using Clock = chrono::steady_clock;

double seconds(Clock::time_point start, Clock::time_point end) {
    return chrono::duration<double>(end - start).count();
}

// Linux reports ru_maxrss in KiB; macOS reports bytes. Read the process
// high-water mark after the timed algorithm, so this call cannot affect its
// reported running time. No periodic memory sampler runs in cp_algo.
uint64_t process_peak_rss_bytes() {
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0)
        throw runtime_error("cannot read process peak RSS");
#if defined(__linux__)
    return (uint64_t)usage.ru_maxrss * 1024;
#elif defined(__APPLE__)
    return (uint64_t)usage.ru_maxrss;
#else
#error "process peak RSS units are defined only for Linux and macOS"
#endif
}

void usage() {
    cerr << "Usage: cp_algo <matching|bfs|dfs|wvc|mvm> graph.ccf "
                 "[--config config.toml]\n";
}

// Figure 8 repeats at most this many rounds for a (3/2 + epsilon) bound.
// The two forms avoid overflow for tiny epsilon and cancellation for large epsilon.
int mvm_round_limit(double epsilon) {
    const long double e = epsilon;
    const long double numerator = e < 1.5L
        ? log(3.0L + 2.0L * e) - log(2.0L) - log(e)
        : log1p(1.5L / e);
    return int(ceil(numerator / log(19.0L / 16.0L)));
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3 && argc != 5) { usage(); return 2; }
        const string algorithm = argv[1];
        if (algorithm != "matching" && algorithm != "bfs" &&
            algorithm != "dfs" && algorithm != "wvc" && algorithm != "mvm") {
            usage(); return 2;
        }
        const filesystem::path input_path = argv[2];
        cp_algo::Config config;
        if (argc == 5) {
            if (string(argv[3]) != "--config") { usage(); return 2; }
            config = cp_algo::load_config(argv[4]);
        }
        if (algorithm != "matching" && config.matching_variant_specified)
            throw runtime_error("matching_variant only applies to matching");
        if (algorithm != "wvc" && algorithm != "mvm" && config.weights_file)
            throw runtime_error("weights_file only applies to wvc and mvm");
        if (algorithm != "mvm" &&
            (config.mvm_epsilon_specified || config.mvm_mode_specified))
            throw runtime_error("mvm_epsilon and mvm_mode only apply to mvm");
        const int round_limit = algorithm == "mvm" ? mvm_round_limit(config.mvm_epsilon) : 0;

        const auto load_start = Clock::now();
        auto ccf = cp_algo::Ccf::load(input_path);
        const double ccf_load_seconds = seconds(load_start, Clock::now());

        vector<vector<int>> C;
        optional<double> cover_decode_seconds;
        if (config.cover_mode == cp_algo::CoverMode::Plain) {
            const auto decode_start = Clock::now();
            C = ccf.decode_all();
            ccf.release_bytes();
            cover_decode_seconds = seconds(decode_start, Clock::now());
        }

        vector<uint64_t> weights;
        optional<double> weight_preparation_seconds;
        if (algorithm == "wvc" || algorithm == "mvm") {
            const auto weight_start = Clock::now();
            weights = config.weights_file ? cp_algo::load_weights(*config.weights_file, ccf.n)
                                          : cp_algo::generated_weights(ccf.n);
            weight_preparation_seconds = seconds(weight_start, Clock::now());
        }

        // The report name is reserved before the measured call. Each sbatch job
        // is one execution; concurrent jobs can safely publish in the same cwd.
        cp_algo::ReportSlot report_slot(algorithm, input_path, config.cover_mode);
        string result_key;
        string result_value;
        bool result_is_decimal_string = false;
        int integer_result = 0;
        __uint128_t weighted_result = 0;
        optional<cp_algo::MvmExecution> mvm_execution;

        const auto algorithm_start = Clock::now();
        if (algorithm == "matching") {
            integer_result = config.cover_mode == cp_algo::CoverMode::Plain
                ? cp_algo::matching_plain(C, ccf.n, config.matching_variant)
                : cp_algo::matching_encoded(ccf, config.matching_variant);
            result_key = "matching_size";
        } else if (algorithm == "bfs") {
            integer_result = config.cover_mode == cp_algo::CoverMode::Plain
                ? cp_algo::bfs_plain(C, ccf.n) : cp_algo::bfs_encoded(ccf);
            result_key = "connected_components";
        } else if (algorithm == "dfs") {
            integer_result = config.cover_mode == cp_algo::CoverMode::Plain
                ? cp_algo::dfs_plain(C, ccf.n) : cp_algo::dfs_encoded(ccf);
            result_key = "connected_components";
        } else if (algorithm == "wvc") {
            weighted_result = config.cover_mode == cp_algo::CoverMode::Plain
                ? cp_algo::wvc_plain(C, weights) : cp_algo::wvc_encoded(ccf, weights);
            result_key = "selected_weight_sum";
        } else {
            const bool extended = config.mvm_mode == cp_algo::MvmMode::UntilNoImprovement;
            const auto outcome = config.cover_mode == cp_algo::CoverMode::Plain
                ? cp_algo::mvm_plain(C, weights, round_limit, extended)
                : cp_algo::mvm_encoded(ccf, weights, round_limit, extended);
            weighted_result = outcome.matched_weight_sum;
            result_key = "matched_vertex_weight_sum";
            const string stop_reason = outcome.stop_reason;
            mvm_execution = cp_algo::MvmExecution{
                config.mvm_epsilon, round_limit, config.mvm_mode,
                outcome.rounds_attempted, outcome.rounds_with_flip,
                outcome.rounds_with_flip < round_limit && stop_reason != "round_limit",
                stop_reason
            };
        }
        const double algorithm_seconds = seconds(algorithm_start, Clock::now());
        const uint64_t peak_rss_bytes = process_peak_rss_bytes();
        if (algorithm == "wvc" || algorithm == "mvm") {
            result_value = cp_algo::decimal(weighted_result);
            result_is_decimal_string = true;
        } else {
            result_value = to_string(integer_result);
        }

        const cp_algo::RunReport report{
            algorithm, config.cover_mode, config.matching_variant, &ccf,
            config.source_path, config.weights_file,
            result_key, result_value, result_is_decimal_string,
            peak_rss_bytes,
            ccf_load_seconds, cover_decode_seconds, weight_preparation_seconds,
            algorithm_seconds, mvm_execution
        };
        report_slot.publish(report);
        cout << report_slot.path().string() << '\n';
        return 0;
    } catch (const exception& error) {
        cerr << "cp_algo: " << error.what() << '\n';
        return 1;
    }
}
