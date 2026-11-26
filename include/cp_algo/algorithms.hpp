#ifndef CP_ALGO_ALGORITHMS_HPP
#define CP_ALGO_ALGORITHMS_HPP

#include "cp_algo/ccf.hpp"

#include <cstdint>
#include <vector>

namespace cp_algo {

enum class MatchingVariant { ThreeHalves, Maximal };

#if !defined(__SIZEOF_INT128__)
#error "cp_algo needs a C++ compiler with 128-bit integer arithmetic"
#endif

int matching_plain(const std::vector<std::vector<int>>& C, int n,
                   MatchingVariant variant);
int matching_encoded(const Ccf& ccf, MatchingVariant variant);

struct MvmResult {
    __uint128_t matched_weight_sum = 0;
    int rounds_attempted = 0;
    int rounds_with_flip = 0;
    const char* stop_reason = "round_limit";
};

MvmResult mvm_plain(const std::vector<std::vector<int>>& C,
                    const std::vector<std::uint64_t>& weights,
                    int round_limit, bool until_no_improvement);
MvmResult mvm_encoded(Ccf& ccf, const std::vector<std::uint64_t>& weights,
                      int round_limit, bool until_no_improvement);

int bfs_plain(const std::vector<std::vector<int>>& C, int n);
int bfs_encoded(const Ccf& ccf);
int dfs_plain(const std::vector<std::vector<int>>& C, int n);
int dfs_encoded(const Ccf& ccf);

__uint128_t wvc_plain(const std::vector<std::vector<int>>& C,
                           const std::vector<std::uint64_t>& weights);
__uint128_t wvc_encoded(const Ccf& ccf,
                             const std::vector<std::uint64_t>& weights);

}  // namespace cp_algo

#endif
