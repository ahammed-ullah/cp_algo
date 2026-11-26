#include "cp_algo/algorithms.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

using namespace std;

namespace cp_algo {

__uint128_t wvc_encoded(const Ccf& ccf,
                              const vector<uint64_t>& weights) {
    // Figure 16 uses the same residual-weight pass. The CCF cursor reads
    // one clique at a time, without retaining a decoded cover.
    vector<uint64_t> residual = weights;
    __uint128_t cover_weight = 0;

    for (int q = 0; q < ccf.num_cliques; ++q) {
        auto clique = ccf.cursor(q);
        int pending = -1;
        int v;
        while (clique.next(v)) {
            if (residual[v] == 0) continue;
            if (pending == -1) {
                pending = v;
                continue;
            }

            uint64_t delta = min(residual[pending], residual[v]);
            residual[pending] -= delta;
            residual[v] -= delta;
            if (residual[pending] == 0) cover_weight += weights[pending];
            if (residual[v] == 0) cover_weight += weights[v];

            if (residual[pending] == 0)
                pending = residual[v] == 0 ? -1 : v;
        }
    }
    return cover_weight;
}

}  // namespace cp_algo
