#include "cp_algo/algorithms.hpp"
#include "cp_algo/encoded_incidence.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

using namespace std;

namespace cp_algo {

int bfs_encoded(const Ccf& ccf) {
    // Figure 9: L[v] holds clique labels; the cliques remain in the CCF.
    auto L = build_encoded_incidence(ccf);

    vector<unsigned char> seen(ccf.n, 0);
    vector<uint8_t> used((static_cast<size_t>(ccf.num_cliques) + 7) / 8, 0);
    vector<int> queue;
    int components = 0;

    for (int root = 0; root < ccf.n; ++root) {
        if (seen[root]) continue;
        ++components;
        seen[root] = 1;
        queue.clear();
        queue.push_back(root);
        int head = 0;

        while (head < (int)queue.size()) {
            int v = queue[head++];
            for (uint32_t pos = L.offsets[v]; pos < L.offsets[v + 1]; ++pos) {
                int q = L.at(pos);
                uint8_t& word = used[q / 8];
                const uint8_t bit = static_cast<uint8_t>(1u << (q % 8));
                if (word & bit) continue;
                word |= bit;

                auto clique = ccf.cursor(q);
                int u;
                while (clique.next(u)) {
                    if (seen[u]) continue;
                    seen[u] = 1;
                    queue.push_back(u);
                }
            }
        }
    }
    return components;
}

}  // namespace cp_algo
