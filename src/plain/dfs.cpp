#include "cp_algo/algorithms.hpp"

#include <vector>

using namespace std;

namespace cp_algo {

int dfs_plain(const vector<vector<int>>& C, int n) {
    // Figure 10: L[v] lists the cliques containing v, and a[q] is the
    // next member of C[q] to consider. All DFS branches share a[q].
    vector<vector<int>> L(n);
    for (int q = 0; q < (int)C.size(); ++q) {
        for (int v : C[q]) L[v].push_back(q);
    }
    vector<int> a(C.size(), 0);
    vector<unsigned char> seen(n, 0);

    struct Frame {
        int v;
        int next_label;
    };
    vector<Frame> stack;
    int components = 0;

    for (int root = 0; root < n; ++root) {
        if (seen[root]) continue;
        ++components;
        seen[root] = 1;
        stack.push_back({root, 0});

        while (!stack.empty()) {
            Frame& frame = stack.back();
            if (frame.next_label == (int)L[frame.v].size()) {
                stack.pop_back();
                continue;
            }

            int q = L[frame.v][frame.next_label];
            int child = -1;
            while (a[q] < (int)C[q].size()) {
                int u = C[q][a[q]++];
                if (!seen[u]) {
                    child = u;
                    break;
                }
            }

            if (child == -1) {
                ++frame.next_label;
            } else {
                // Resume this clique after the child's whole DFS subtree.
                seen[child] = 1;
                stack.push_back({child, 0});
            }
        }
    }
    return components;
}

}  // namespace cp_algo
