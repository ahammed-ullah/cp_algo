#ifndef CP_ALGO_REPORT_HPP
#define CP_ALGO_REPORT_HPP

#include "cp_algo/ccf.hpp"
#include "cp_algo/config.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace cp_algo {

struct MvmExecution {
    double epsilon;
    int round_limit;
    MvmMode mode;
    int rounds_attempted;
    int rounds_with_flip;
    bool early_exit;
    std::string stop_reason;
};

struct RunReport {
    std::string algorithm;
    CoverMode cover_mode;
    MatchingVariant matching_variant;
    const Ccf* ccf;
    std::optional<std::filesystem::path> config_path;
    std::optional<std::filesystem::path> weights_file;
    std::string result_key;
    std::string result_value;
    bool result_is_decimal_string = false;
    std::uint64_t process_peak_rss_bytes = 0;
    double ccf_load_seconds = 0;
    std::optional<double> cover_decode_seconds;
    std::optional<double> weight_preparation_seconds;
    double algorithm_seconds = 0;
    std::optional<MvmExecution> mvm_execution;
};

class ReportSlot {
public:
    ReportSlot(const std::string& algorithm, const std::filesystem::path& graph,
               CoverMode mode);
    ReportSlot(const ReportSlot&) = delete;
    ReportSlot& operator=(const ReportSlot&) = delete;
    ~ReportSlot();

    const std::filesystem::path& path() const { return report_path_; }
    void publish(const RunReport& report);

private:
    std::filesystem::path report_path_;
    std::filesystem::path claim_path_;
    bool published_ = false;
};

}  // namespace cp_algo

#endif
