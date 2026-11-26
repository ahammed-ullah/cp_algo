#include "cp_algo/algorithms.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

using namespace std;

namespace cp_algo {

__uint128_t wvc_plain(const vector<vector<int>>& C,
                            const vector<uint64_t>& weights) {
    // Figure 16: residual[v] is the unassigned part of vertex weight w[v].
    // A vertex enters the cover when its residual first reaches zero.
    vector<uint64_t> residual = weights;
    __uint128_t cover_weight = 0;

    for (const auto& clique : C) {
        int pending = -1;
        for (int v : clique) {
            if (residual[v] == 0) continue;
            if (pending == -1) {
                pending = v;
                continue;
            }

            // Give the edge (pending,v) as much dual weight as possible.
            uint64_t delta = min(residual[pending], residual[v]);
            residual[pending] -= delta;
            residual[v] -= delta;
            if (residual[pending] == 0) cover_weight += weights[pending];
            if (residual[v] == 0) cover_weight += weights[v];

            // At most one vertex of the pair still has positive residual.
            if (residual[pending] == 0)
                pending = residual[v] == 0 ? -1 : v;
        }
    }
    return cover_weight;
}

}  // namespace cp_algo
