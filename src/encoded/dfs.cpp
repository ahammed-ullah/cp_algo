#include "cp_algo/algorithms.hpp"
#include "cp_algo/encoded_incidence.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

using namespace std;

namespace cp_algo {

int dfs_encoded(const Ccf& ccf) {
    // Figure 10: L[v] contains packed clique labels. Each a[q] keeps just
    // the byte position and previous vertex needed to resume CCF decoding.
    auto L = build_encoded_incidence(ccf);

    if (ccf.file_bytes > UINT32_MAX)
        throw runtime_error("encoded DFS exceeds 32-bit CCF positions");

    vector<Ccf::CompactCliqueCursor> a;
    a.reserve(ccf.num_cliques);
    for (int q = 0; q < ccf.num_cliques; ++q) a.push_back(ccf.compact_cursor(q));
    vector<unsigned char> seen(ccf.n, 0);

    struct Frame {
        int v;
        uint32_t next_label;
        int active_label;  // -1 until the next label is read from L[v].
    };
    vector<Frame> stack;
    int components = 0;

    for (int root = 0; root < ccf.n; ++root) {
        if (seen[root]) continue;
        ++components;
        seen[root] = 1;
        stack.push_back({root, L.offsets[root], -1});

        while (!stack.empty()) {
            Frame& frame = stack.back();
            if (frame.active_label == -1) {
                if (frame.next_label == L.offsets[frame.v + 1]) {
                    stack.pop_back();
                    continue;
                }
                frame.active_label = L.at(frame.next_label++);
            }

            int q = frame.active_label;
            int child = -1;
            int u;
            while (ccf.next_compact(q, a[q], u)) {
                if (!seen[u]) {
                    child = u;
                    break;
                }
            }

            if (child == -1) {
                frame.active_label = -1;
            } else {
                // Keep q active so its cursor resumes after the child.
                seen[child] = 1;
                stack.push_back({child, L.offsets[child], -1});
            }
        }
    }
    return components;
}

}  // namespace cp_algo
