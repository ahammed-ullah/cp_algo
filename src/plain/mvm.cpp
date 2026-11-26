#include "cp_algo/algorithms.hpp"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace std;

namespace cp_algo {
namespace {

// One record in beta[ell] is a terminal vertex z. Its matched partner is
// mate[z], which does not change while Figure 7 packs paths.
struct BetaState {
    int next;
    int end;
    int top[3];
};

// The orientation is (u, mate[u]). Its two witnesses determine the gain;
// storing the gain itself would require another word per orientation.
struct Witness {
    int source;
    int terminal;
};

// The remaining vertices of the path are mate[center] and, for a length-4
// path, mate[terminal]. They remain unchanged until all paths are flipped.
struct ChosenPath {
    int source;
    int center;
    int terminal;
};

void matching_edges(const vector<int>& mate, vector<pair<int, int>>& edges) {
    edges.clear();
    for (int u = 0; u < static_cast<int>(mate.size()); ++u)
        if (mate[u] > u) edges.emplace_back(u, mate[u]);
}

// Initially Figure 5 pairs vertices while the first pass counts L_v sizes.
// The second pass writes clique labels into one flat incidence array.
void initial_matching_and_incidence(const vector<vector<int>>& C, int n,
                                    vector<int>& mate, vector<int>& cand,
                                    vector<int>& L_start, vector<int>& L_label) {
    L_start.assign(n + 1, 0);
    for (int ell = 0; ell < static_cast<int>(C.size()); ++ell) {
        int pending = -1;
        for (int v : C[ell]) {
            ++L_start[v + 1];
            if (mate[v] != -1) continue;
            if (pending == -1) {
                pending = v;
            } else {
                mate[pending] = v;
                mate[v] = pending;
                pending = -1;
            }
        }
        // A later clique may match this vertex. Read cand only with mate.
        cand[ell] = pending;
    }

    for (int v = 0; v < n; ++v) L_start[v + 1] += L_start[v];
    L_label.resize(L_start[n]);
    {
        vector<int> next(L_start.begin(), L_start.end() - 1);
        for (int ell = 0; ell < static_cast<int>(C.size()); ++ell)
            for (int v : C[ell]) L_label[next[v]++] = ell;
    }
}

// An even path trades its free source for a lighter, newly free terminal.
// Old free vertices are already represented by cand; only these new free
// vertices need to be visited to make the matching maximal again.
void restore_maximality(const vector<int>& released,
                        const vector<int>& L_start, const vector<int>& L_label,
                        vector<int>& mate, vector<int>& cand) {
    for (int z : released) {
        if (mate[z] != -1) continue;
        for (int p = L_start[z]; p < L_start[z + 1]; ++p) {
            const int ell = L_label[p];
            const int x = cand[ell];
            if (x != -1 && x != z && mate[x] == -1) {
                mate[x] = z;
                mate[z] = x;
                cand[ell] = -1;
                break;
            }
            cand[ell] = z;
        }
    }
}

// For one matched endpoint, two distinct free neighbors suffice to decide
// whether its matching edge has a length-3 augmenting path.
pair<int, int> free_neighbors(int u, const vector<int>& L_start,
                              const vector<int>& L_label,
                              const vector<int>& cand,
                              const vector<int>& mate) {
    int first = -1, second = -1;
    for (int p = L_start[u]; p < L_start[u + 1]; ++p) {
        const int x = cand[L_label[p]];
        if (x == -1 || mate[x] != -1 || x == first) continue;
        if (first == -1) first = x;
        else { second = x; break; }
    }
    return {first, second};
}

// Figure 6's single sweep, using L_v and cand in place of Gamma_v.
// Newly inserted edges touch vertices that were free in the maximal M0;
// those vertices had no free neighbors in M0, so one sweep still suffices.
void repair_length_three(const vector<pair<int, int>>& M0,
                         const vector<int>& L_start,
                         const vector<int>& L_label,
                         const vector<int>& cand, vector<int>& mate) {
    for (const auto& [u, v] : M0) {
        const auto [first_u, second_u] =
            free_neighbors(u, L_start, L_label, cand, mate);
        if (first_u == -1) continue;
        const auto [first_v, second_v] =
            free_neighbors(v, L_start, L_label, cand, mate);
        if (first_v == -1) continue;

        int x = first_u, y = first_v;
        if (x == y) {
            if (second_u != -1) x = second_u;
            else y = second_v;
        }
        if (y == -1) continue;

        mate[u] = x;
        mate[x] = u;
        mate[v] = y;
        mate[y] = v;
    }
}

// Each clique contains zero or one free vertex after cardinality repair.
// Thus its beta size is |C_ell| minus that free vertex, when present. A
// global weight-ordered terminal scan then appends sorted beta records.
void build_beta(const vector<vector<int>>& C, const vector<int>& order,
                const vector<int>& L_start, const vector<int>& L_label,
                const vector<int>& mate, const vector<int>& cand,
                vector<int>& offset, vector<int>& record,
                vector<BetaState>& state) {
    const int q = static_cast<int>(C.size());
    offset[0] = 0;
    for (int ell = 0; ell < q; ++ell) {
        const int x = cand[ell];
        const int free_member = x != -1 && mate[x] == -1;
        offset[ell + 1] = offset[ell] +
                          static_cast<int>(C[ell].size()) - free_member;
    }
    record.resize(offset[q]);
    for (int ell = 0; ell < q; ++ell) {
        state[ell].next = offset[ell];
        state[ell].end = offset[ell + 1];
        for (int& z : state[ell].top) z = -1;
    }

    for (int z : order) {
        const int y = mate[z];
        if (y == -1) continue;
        for (int p = L_start[y]; p < L_start[y + 1]; ++p) {
            const int ell = L_label[p];
            record[state[ell].next++] = z;
        }
    }
    for (int ell = 0; ell < q; ++ell) state[ell].next = offset[ell];
}

// Keep the first three active records without deleting from the flat list.
// Each record is read or skipped at most once during a packing call.
void refill(BetaState& state, const vector<int>& record,
            const vector<unsigned char>& active) {
    int kept = 0;
    for (int i = 0; i < 3; ++i) {
        const int z = state.top[i];
        if (z != -1 && active[z]) state.top[kept++] = z;
    }
    while (kept < 3 && state.next < state.end) {
        const int z = record[state.next++];
        if (active[z]) state.top[kept++] = z;
    }
    while (kept < 3) state.top[kept++] = -1;
}

int best_source(int u, const vector<int>& L_start,
                const vector<int>& L_label, const vector<int>& cand,
                const vector<int>& mate,
                const vector<unsigned char>& active,
                const vector<uint64_t>& weights) {
    int best = -1;
    for (int p = L_start[u]; p < L_start[u + 1]; ++p) {
        const int x = cand[L_label[p]];
        if (x == -1 || mate[x] != -1 || !active[x]) continue;
        if (best == -1 || weights[x] > weights[best] ||
            (weights[x] == weights[best] && x < best)) best = x;
    }
    return best;
}

int best_terminal(int u, int v, const vector<int>& L_start,
                  const vector<int>& L_label, vector<BetaState>& state,
                  const vector<int>& record,
                  const vector<unsigned char>& active,
                  const vector<uint64_t>& weights) {
    int best = v;  // The length-2 path is always a candidate.
    for (int p = L_start[v]; p < L_start[v + 1]; ++p) {
        BetaState& beta = state[L_label[p]];
        refill(beta, record, active);
        for (int z : beta.top) {
            if (z == -1) break;
            if (z == u || z == v) continue;  // Exclude the center edge.
            if (weights[z] < weights[best] ||
                (weights[z] == weights[best] && z < best)) best = z;
            break;  // Later records in this clique are no lighter.
        }
    }
    return best;
}

uint64_t gain(const Witness& witness, const vector<uint64_t>& weights) {
    if (witness.source == -1 || witness.terminal == -1 ||
        weights[witness.source] <= weights[witness.terminal]) return 0;
    return weights[witness.source] - weights[witness.terminal];
}

void try_orientation(int u, int v, uint64_t threshold,
                     const vector<int>& L_start,
                     const vector<int>& L_label, const vector<int>& cand,
                     const vector<int>& mate, vector<BetaState>& beta,
                     const vector<int>& record,
                     const vector<uint64_t>& weights,
                     vector<unsigned char>& active,
                     vector<Witness>& witnesses,
                     vector<ChosenPath>& paths,
                     __uint128_t& packed_gain) {
    if (!active[u]) return;
    Witness& witness = witnesses[u];
    if (witness.source == -1 || gain(witness, weights) < threshold) return;

    // A surviving source or terminal is still optimal. If a witness dies,
    // its old gain bounds the new gain from above, so the test above may
    // avoid both incidence-list scans.
    if (!active[witness.source])
        witness.source = best_source(u, L_start, L_label, cand, mate,
                                     active, weights);
    if (witness.source == -1) return;
    if (!active[witness.terminal])
        witness.terminal = best_terminal(u, v, L_start, L_label, beta,
                                         record, active, weights);
    const uint64_t current_gain = gain(witness, weights);
    if (current_gain < threshold) return;

    const int x = witness.source;
    const int z = witness.terminal;
    paths.push_back({x, u, z});
    packed_gain += current_gain;
    active[x] = 0;
    active[u] = 0;
    active[v] = 0;
    if (z != v) {
        active[z] = 0;
        active[mate[z]] = 0;
    }
}

void flip_paths(const vector<ChosenPath>& paths, vector<int>& mate,
                vector<int>& released) {
    released.clear();
    for (const ChosenPath& path : paths) {
        const int x = path.source;
        const int u = path.center;
        const int v = mate[u];
        const int z = path.terminal;
        if (z == v) {
            mate[x] = u;
            mate[u] = x;
            mate[v] = -1;
        } else {
            const int y = mate[z];
            mate[x] = u;
            mate[u] = x;
            mate[v] = y;
            mate[y] = v;
            mate[z] = -1;
        }
        released.push_back(z);
    }
}

}  // namespace

MvmResult mvm_plain(const vector<vector<int>>& C,
                    const vector<uint64_t>& weights,
                    int round_limit, bool until_no_improvement) {
    const int n = static_cast<int>(weights.size());
    const int q = static_cast<int>(C.size());
    vector<int> mate(n, -1), cand(q, -1), L_start, L_label;
    initial_matching_and_incidence(C, n, mate, cand, L_start, L_label);

    vector<pair<int, int>> edges;
    vector<int> released, order, beta_offset, beta_record;
    vector<BetaState> beta;
    vector<Witness> witnesses;
    vector<unsigned char> active;
    vector<ChosenPath> paths;
    MvmResult result;

    auto finish = [&](const char* reason) {
        result.stop_reason = reason;
        for (int v = 0; v < n; ++v)
            if (mate[v] != -1) result.matched_weight_sum += weights[v];
        return result;
    };

    for (;;) {
        ++result.rounds_attempted;
        if (result.rounds_attempted > 1)
            restore_maximality(released, L_start, L_label, mate, cand);

        matching_edges(mate, edges);
        repair_length_three(edges, L_start, L_label, cand, mate);
        matching_edges(mate, edges);
        const int mu = static_cast<int>(edges.size());
        const int free_count = n - 2 * mu;
        if (free_count == 0) return finish("all_vertices_matched");
        if (edges.empty()) return finish("no_positive_gain");

        uint64_t max_free = 0, min_matched = UINT64_MAX;
        for (int v = 0; v < n; ++v) {
            if (mate[v] == -1) max_free = max(max_free, weights[v]);
            else min_matched = min(min_matched, weights[v]);
        }
        if (max_free <= min_matched) return finish("weight_bound");

        // Sorting is delayed until a round actually needs beta. An early
        // return after cardinality repair avoids this one-time cost.
        if (order.empty()) {
            order.resize(n);
            iota(order.begin(), order.end(), 0);
            sort(order.begin(), order.end(), [&](int a, int b) {
                return weights[a] < weights[b] ||
                       (weights[a] == weights[b] && a < b);
            });
            beta_offset.resize(q + 1);
            beta.resize(q);
            witnesses.assign(n, {-1, -1});
            active.resize(n);
        }

        build_beta(C, order, L_start, L_label, mate, cand,
                   beta_offset, beta_record, beta);
        fill(active.begin(), active.end(), 1);

        uint64_t gmax = 0;
        for (const auto& [a, b] : edges) {
            for (int u : {a, b}) {
                const int v = mate[u];
                Witness& witness = witnesses[u];
                witness.source = best_source(u, L_start, L_label, cand,
                                             mate, active, weights);
                witness.terminal = witness.source == -1 ? -1 :
                    best_terminal(u, v, L_start, L_label, beta,
                                  beta_record, active, weights);
                gmax = max(gmax, gain(witness, weights));
            }
        }
        if (gmax == 0) return finish("no_positive_gain");

        const int a = min(free_count, mu);
        const bool full_thresholds = until_no_improvement &&
                                     result.rounds_with_flip >= round_limit;
        __uint128_t packed_gain = 0;
        paths.clear();
        uint64_t threshold = gmax;
        for (;;) {
            for (const auto& [u, v] : edges) {
                try_orientation(u, v, threshold, L_start, L_label, cand,
                                mate, beta, beta_record, weights, active,
                                witnesses, paths, packed_gain);
                try_orientation(v, u, threshold, L_start, L_label, cand,
                                mate, beta, beta_record, weights, active,
                                witnesses, paths, packed_gain);
            }
            if (threshold == 1) break;
            if (!full_thresholds &&
                __uint128_t(3) * a * (threshold - 1) <= packed_gain) break;
            threshold = (threshold >> 1) + (threshold & 1);  // ceil(t/2)
        }
        if (paths.empty())
            throw logic_error("positive MVM gain produced no selected path");

        flip_paths(paths, mate, released);
        ++result.rounds_with_flip;
        if (!until_no_improvement && result.rounds_with_flip == round_limit)
            return finish("round_limit");
    }
}

}  // namespace cp_algo
