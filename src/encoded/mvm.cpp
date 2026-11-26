#include "cp_algo/algorithms.hpp"
#include "cp_algo/encoded_incidence.hpp"
#include "cp_algo/vbyte.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
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

// Figure 7's terminal IDs are in weight order, not vertex-ID order. Fixed-
// width packing retains direct access by record number for each beta cursor.
struct PackedBeta {
    vector<uint8_t> bytes;
    unsigned width = 1;

    void reset(int records) {
        bytes.resize((static_cast<size_t>(records) * width + 7) / 8);
        // put() uses OR because adjacent records can share a byte. Old bits
        // must be cleared when beta is rebuilt for a new matching round.
        fill(bytes.begin(), bytes.end(), 0);
    }

    void put(int slot, int vertex) {
        const size_t bit = static_cast<size_t>(slot) * width;
        const size_t byte = bit / 8;
        const unsigned shift = static_cast<unsigned>(bit % 8);
        const uint64_t value = static_cast<uint64_t>(vertex) << shift;
        const unsigned count = (width + shift + 7) / 8;
        for (unsigned i = 0; i < count; ++i)
            bytes[byte + i] |= static_cast<uint8_t>(value >> (8 * i));
    }

    int at(int slot) const {
        const size_t bit = static_cast<size_t>(slot) * width;
        const size_t byte = bit / 8;
        const unsigned shift = static_cast<unsigned>(bit % 8);
        const unsigned count = (width + shift + 7) / 8;
        uint64_t value = 0;
        for (unsigned i = 0; i < count; ++i)
            value |= static_cast<uint64_t>(bytes[byte + i]) << (8 * i);
        return static_cast<int>((value >> shift) & ((1u << width) - 1u));
    }
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

// The first CCF scan executes Figure 5, counts each L_v, and saves only the
// clique cardinalities needed for beta in later rounds. The second scan fills
// packed L_v in increasing clique-label order, then CCF bytes are released.
// If Figure 5 already covers every vertex, no L_v or second scan is needed.
bool initial_matching_and_incidence(Ccf& ccf, vector<int>& mate,
                                    vector<int>& cand, EncodedIncidence& L,
                                    vector<uint8_t>& clique_sizes) {
    if (ccf.cover_size > numeric_limits<uint32_t>::max())
        throw runtime_error("encoded incidence exceeds 32-bit offsets");
    L.offsets.assign(static_cast<size_t>(ccf.n) + 1, 0);
    while ((1u << L.width) < static_cast<unsigned>(ccf.num_cliques))
        ++L.width;
    clique_sizes.reserve(ccf.num_cliques);
    int matched_vertices = 0;
    for (int ell = 0; ell < ccf.num_cliques; ++ell) {
        auto clique = ccf.cursor(ell);
        int pending = -1;
        uint32_t size = 0;
        int v;
        while (clique.next(v)) {
            ++size;
            ++L.offsets[v];
            if (mate[v] != -1) continue;
            if (pending == -1) {
                pending = v;
            } else {
                mate[pending] = v;
                mate[v] = pending;
                matched_vertices += 2;
                pending = -1;
            }
        }
        // A later clique may match this vertex. Read cand only with mate.
        cand[ell] = pending;
        append_vbyte(clique_sizes, size);
    }

    if (matched_vertices == ccf.n) {
        ccf.release_bytes();
        return true;
    }

    uint32_t total = 0;
    for (int v = 0; v < ccf.n; ++v) {
        total += L.offsets[v];
        L.offsets[v] = total;
    }
    L.offsets[ccf.n] = total;
    L.labels.resize((static_cast<size_t>(total) * L.width + 7) / 8);
    // Filling backward uses each end position as the write pointer and
    // leaves the final starts in L.offsets, as in encoded BFS and DFS.
    for (int ell = ccf.num_cliques; ell-- > 0;) {
        auto clique = ccf.cursor(ell);
        int v;
        while (clique.next(v)) L.put(--L.offsets[v], ell);
    }
    ccf.release_bytes();
    return false;
}

// An even path trades its free source for a lighter, newly free terminal.
// Old free vertices are already represented by cand; only these new free
// vertices need to be visited to make the matching maximal again.
void restore_maximality(const vector<ChosenPath>& paths,
                        const EncodedIncidence& L,
                        vector<int>& mate, vector<int>& cand) {
    // A path's terminal is precisely the vertex released by its flip.
    for (const ChosenPath& path : paths) {
        const int z = path.terminal;
        if (mate[z] != -1) continue;
        for (uint32_t p = L.offsets[z]; p < L.offsets[z + 1]; ++p) {
            const int ell = L.at(p);
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
pair<int, int> free_neighbors(int u, const EncodedIncidence& L,
                              const vector<int>& cand,
                              const vector<int>& mate) {
    int first = -1, second = -1;
    for (uint32_t p = L.offsets[u]; p < L.offsets[u + 1]; ++p) {
        const int x = cand[L.at(p)];
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
                         const EncodedIncidence& L,
                         const vector<int>& cand, vector<int>& mate) {
    for (const auto& [u, v] : M0) {
        const auto [first_u, second_u] =
            free_neighbors(u, L, cand, mate);
        if (first_u == -1) continue;
        const auto [first_v, second_v] =
            free_neighbors(v, L, cand, mate);
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
// The saved cardinalities yield beta lengths without reading CCF again.
// Scanning matched terminals in global weight order fills sorted beta lists.
void build_beta(const vector<uint8_t>& clique_sizes,
                const vector<int>& order, const EncodedIncidence& L,
                const vector<int>& mate, const vector<int>& cand,
                PackedBeta& record, vector<BetaState>& state) {
    const int q = static_cast<int>(state.size());
    size_t size_pos = 0;
    int end = 0;
    for (int ell = 0; ell < q; ++ell) {
        const int size = static_cast<int>(read_vbyte(clique_sizes, size_pos));
        const int x = cand[ell];
        const int free_member = x != -1 && mate[x] == -1;
        state[ell].next = end;
        end += size - free_member;
        state[ell].end = end;
        for (int& z : state[ell].top) z = -1;
    }
    record.reset(end);

    for (int z : order) {
        const int y = mate[z];
        if (y == -1) continue;
        for (uint32_t p = L.offsets[y]; p < L.offsets[y + 1]; ++p) {
            const int ell = L.at(p);
            record.put(state[ell].next++, z);
        }
    }
    // The previous clique's end is this clique's beginning. No separate
    // beta-offset array is kept after the packed lists have been filled.
    int begin = 0;
    for (int ell = 0; ell < q; ++ell) {
        state[ell].next = begin;
        begin = state[ell].end;
    }
}

// Keep the first three active records without deleting from the flat list.
// Each record is read or skipped at most once during a packing call.
void refill(BetaState& state, const PackedBeta& record,
            const vector<unsigned char>& active) {
    int kept = 0;
    for (int i = 0; i < 3; ++i) {
        const int z = state.top[i];
        if (z != -1 && active[z]) state.top[kept++] = z;
    }
    while (kept < 3 && state.next < state.end) {
        const int z = record.at(state.next++);
        if (active[z]) state.top[kept++] = z;
    }
    while (kept < 3) state.top[kept++] = -1;
}

int best_source(int u, const EncodedIncidence& L, const vector<int>& cand,
                const vector<int>& mate,
                const vector<unsigned char>& active,
                const vector<uint64_t>& weights) {
    int best = -1;
    for (uint32_t p = L.offsets[u]; p < L.offsets[u + 1]; ++p) {
        const int x = cand[L.at(p)];
        if (x == -1 || mate[x] != -1 || !active[x]) continue;
        if (best == -1 || weights[x] > weights[best] ||
            (weights[x] == weights[best] && x < best)) best = x;
    }
    return best;
}

int best_terminal(int u, int v, const EncodedIncidence& L,
                  vector<BetaState>& state, const PackedBeta& record,
                  const vector<unsigned char>& active,
                  const vector<uint64_t>& weights) {
    int best = v;  // The length-2 path is always a candidate.
    for (uint32_t p = L.offsets[v]; p < L.offsets[v + 1]; ++p) {
        BetaState& beta = state[L.at(p)];
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
                     const EncodedIncidence& L, const vector<int>& cand,
                     const vector<int>& mate, vector<BetaState>& beta,
                     const PackedBeta& record,
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
        witness.source = best_source(u, L, cand, mate,
                                     active, weights);
    if (witness.source == -1) return;
    if (!active[witness.terminal])
        witness.terminal = best_terminal(u, v, L, beta,
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

void flip_paths(const vector<ChosenPath>& paths, vector<int>& mate) {
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
    }
}

}  // namespace

MvmResult mvm_encoded(Ccf& ccf, const vector<uint64_t>& weights,
                      int round_limit, bool until_no_improvement) {
    const int n = ccf.n;
    const int q = ccf.num_cliques;
    vector<int> mate(n, -1), cand(q, -1);
    EncodedIncidence L;
    vector<uint8_t> clique_sizes;
    const bool initially_perfect = initial_matching_and_incidence(
        ccf, mate, cand, L, clique_sizes);

    vector<pair<int, int>> edges;
    vector<int> order;
    PackedBeta beta_record;
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

    // Figure 5 already gives a perfect matching. The plain mode reaches the
    // same stop after its first Figure 6 pass, which cannot change it.
    if (initially_perfect) {
        result.rounds_attempted = 1;
        return finish("all_vertices_matched");
    }

    for (;;) {
        ++result.rounds_attempted;
        if (result.rounds_attempted > 1)
            restore_maximality(paths, L, mate, cand);

        matching_edges(mate, edges);
        repair_length_three(edges, L, cand, mate);
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
            while ((1u << beta_record.width) < static_cast<unsigned>(n))
                ++beta_record.width;
            beta.resize(q);
            witnesses.assign(n, {-1, -1});
            active.resize(n);
        }

        build_beta(clique_sizes, order, L, mate, cand, beta_record, beta);
        fill(active.begin(), active.end(), 1);

        uint64_t gmax = 0;
        for (const auto& [a, b] : edges) {
            for (int u : {a, b}) {
                const int v = mate[u];
                Witness& witness = witnesses[u];
                witness.source = best_source(u, L, cand,
                                             mate, active, weights);
                witness.terminal = witness.source == -1 ? -1 :
                    best_terminal(u, v, L, beta,
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
                try_orientation(u, v, threshold, L, cand,
                                mate, beta, beta_record, weights, active,
                                witnesses, paths, packed_gain);
                try_orientation(v, u, threshold, L, cand,
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

        flip_paths(paths, mate);
        ++result.rounds_with_flip;
        if (!until_no_improvement && result.rounds_with_flip == round_limit)
            return finish("round_limit");
    }
}

}  // namespace cp_algo
