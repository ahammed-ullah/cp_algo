#ifndef CP_ALGO_CONFIG_HPP
#define CP_ALGO_CONFIG_HPP

#include "cp_algo/algorithms.hpp"

#include <filesystem>
#include <optional>

namespace cp_algo {

enum class CoverMode { Plain, Encoded };
enum class MvmMode { Bounded, UntilNoImprovement };

struct Config {
    CoverMode cover_mode = CoverMode::Plain;
    MatchingVariant matching_variant = MatchingVariant::ThreeHalves;
    bool matching_variant_specified = false;
    double mvm_epsilon = 0.01;
    bool mvm_epsilon_specified = false;
    MvmMode mvm_mode = MvmMode::Bounded;
    bool mvm_mode_specified = false;
    std::optional<std::filesystem::path> weights_file;
    std::optional<std::filesystem::path> source_path;
};

Config load_config(const std::filesystem::path& path);

}  // namespace cp_algo

#endif
