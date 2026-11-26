#include "cp_algo/algorithms.hpp"

#include <vector>

using namespace std;

namespace cp_algo {

int bfs_plain(const vector<vector<int>>& C, int n) {
    // Figure 9: L[v] lists the cliques containing v.
    vector<vector<int>> L(n);
    for (int q = 0; q < (int)C.size(); ++q) {
        for (int v : C[q]) L[v].push_back(q);
    }

    vector<unsigned char> seen(n, 0);
    vector<unsigned char> used(C.size(), 0);
    vector<int> queue;
    int components = 0;

    for (int root = 0; root < n; ++root) {
        if (seen[root]) continue;
        ++components;
        seen[root] = 1;
        queue.clear();
        queue.push_back(root);
        int head = 0;

        while (head < (int)queue.size()) {
            int v = queue[head++];
            for (int q : L[v]) {
                if (used[q]) continue;
                used[q] = 1;

                // The first reached member of C[q] discovers all its
                // unseen members at the next BFS distance.
                for (int u : C[q]) {
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
