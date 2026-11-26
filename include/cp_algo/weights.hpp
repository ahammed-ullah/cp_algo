#ifndef CP_ALGO_WEIGHTS_HPP
#define CP_ALGO_WEIGHTS_HPP

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cp_algo {

constexpr std::uint32_t default_weight_seed = 19u;

// The fixed mt19937_64 and rejection mapping are specified in docs/weights.md.
std::vector<std::uint64_t> generated_weights(int n);
std::vector<std::uint64_t> load_weights(const std::filesystem::path& path, int n);

// JSON has no portable unsigned-128 number type; emit the exact decimal value.
std::string decimal(__uint128_t value);

}  // namespace cp_algo

#endif
