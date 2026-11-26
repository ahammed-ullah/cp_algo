#include "cp_algo/algorithms.hpp"

#include <utility>
#include <vector>

using namespace std;

namespace cp_algo {

int matching_plain(const vector<vector<int>>& C, int n, MatchingVariant variant) {
    vector<int> mate(n, -1);
    int matching_size = 0;

    if (variant == MatchingVariant::Maximal) {
        // Figure 5: pair free vertices in each clique in vertex order.
        for (const auto& clique : C) {
            int pending = -1;
            for (int v : clique) {
                if (mate[v] != -1) continue;
                if (pending == -1) {
                    pending = v;
                } else {
                    mate[pending] = v;
                    mate[v] = pending;
                    ++matching_size;
                    pending = -1;
                }
            }
        }
        return matching_size;
    }

    // Figure 6 starts with the same maximal matching M0. Gamma[v] records
    // free neighbors of v as they arise; M0 holds its edges for one sweep.
    vector<vector<int>> gamma(n);
    vector<pair<int, int>> M0;
    for (const auto& clique : C) {
        int pending = -1;
        for (int v : clique) {
            if (mate[v] != -1) continue;
            if (pending == -1) {
                pending = v;
            } else {
                mate[pending] = v;
                mate[v] = pending;
                M0.emplace_back(pending, v);
                ++matching_size;
                pending = -1;
            }
        }

        // At most one free member remains. It is adjacent to every other
        // member of this clique. Later matches make some entries stale.
        if (pending != -1) {
            for (int v : clique) {
                if (v != pending) gamma[v].push_back(pending);
            }
        }
    }

    // Figure 6: replace (u,v) in M0 by (u,x),(v,y) when x and y are
    // distinct free neighbors. Two choices per endpoint suffice.
    for (const auto& [u, v] : M0) {
        if (mate[u] != v) continue;
        int x = -1, other_x = -1;
        int y = -1, other_y = -1;
        for (int w : gamma[u]) {
            if (mate[w] != -1) continue;
            if (x == -1) x = w;
            else if (w != x) {
                other_x = w;
                break;
            }
        }
        for (int w : gamma[v]) {
            if (mate[w] != -1) continue;
            if (y == -1) y = w;
            else if (w != y) {
                other_y = w;
                break;
            }
        }
        if (x == -1 || y == -1) continue;
        if (x == y) {
            if (other_x != -1) x = other_x;
            else y = other_y;
        }
        if (y == -1) continue;

        mate[u] = x;
        mate[x] = u;
        mate[v] = y;
        mate[y] = v;
        ++matching_size;
    }
    return matching_size;
}

}  // namespace cp_algo
