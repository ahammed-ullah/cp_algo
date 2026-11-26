# Vertex-weighted matching

Section 4.2 defines the weight of a matching as the sum of the weights of
its matched vertices. The `mvm` command implements the approximation from
Figures 5–8 in plain and encoded cover modes. It reports the matched-vertex
weight sum.
[*Concise Clique Covers for Parsimonious Graph Algorithms*](https://arxiv.org/abs/2604.28096) · [Plain implementation](../src/plain/mvm.cpp) · [Encoded implementation](../src/encoded/mvm.cpp)

Here `m` is the number of graph edges, `M` is the current matching,
`μ = |M|`, `f = n − 2μ` is the number of free vertices, and
`σ(C) = Σ_l |C_l|`. `mate[v] = -1` means that `v` is free. The code accepts
positive unsigned 64-bit integer weights, so an increasing path has a
positive integer gain.

## Figure 8: carry one matching through the rounds

Figure 8 starts with an empty matching. In each round it first removes
augmenting paths of length 1 and 3, then calls Figure 7 to pack
vertex-disjoint increasing paths of length 2 or 4. The packed paths are
flipped together. The code keeps the resulting `mate` array for the next
round; it does not restart cardinality matching. The matched-vertex weight
never decreases.

The default is `mvm_epsilon = 0.01`. With the configured `ε > 0`, the
bounded policy permits at most

```text
T = ceil(log((3 + 2ε) / (2ε)) / log(19 / 16))
```

rounds. The default gives `T = 30`. If a round finds no positive increasing
path, it returns before reaching `T`. The optional
`mvm_mode = "until_no_improvement"` runs the same bounded prefix and then
continues until such a round occurs. Its final matching has the paper's
`3/2` certificate. The number of additional rounds has no useful runtime
bound, so bounded mode is the default for experiments.

```toml
mvm_epsilon = 0.01
mvm_mode = "bounded"             # or "until_no_improvement"
weights_file = "weights.txt"     # optional; relative to this config file
```

These keys apply to `mvm`. Set `cover_mode = "encoded"` to run the same
algorithm from packed incidence lists. The [weight guide](weights.md)
specifies generated and file weights.

## Figures 5 and 6: repair cardinality before packing

The first cover pass counts incidences and pairs free members of each
clique as in Figure 5. The second pass fills the clique-incidence lists
`L_v = {l : v ∈ C_l}` in one flat array with per-vertex boundaries. Encoded
mode returns after the first pass if the pairing has matched every vertex.
For each clique `l`, `cand[l]` holds its remaining free vertex, or `-1`.
A vertex stored there can be matched in a later clique, so every read
checks `mate[cand[l]] == -1`. When the initial pass finishes, each clique
contains at most one free vertex and `M` is maximal.

After a Figure 7 packing is flipped, only its terminal vertices `z` can
have become newly free. To restore maximality in the next round, the code
visits `L_z` for each such terminal that is still free. At an incident clique
`l`, it pairs `z` with a different, still-free `cand[l]` if one exists;
otherwise it stores `z` in `cand[l]`. It stops visiting `L_z` once `z` is
matched. Earlier entries that now name `z` are harmless stale entries.
Several released terminals may visit the same clique: the first becomes its
candidate, the next can pair with it, and another can become its new
candidate. This reads incidence labels and `cand` entries, not clique
members. At completion every clique again has at most one free vertex.

```text
for z in terminals released by the previous packing:
    if z is still free:
        for l in L_z, until z is matched:
            x = cand[l]
            if x >= 0 and x != z and mate[x] == -1:
                match x with z; cand[l] = -1
            else: cand[l] = z
```

Figure 6 then removes length-3 augmenting paths. Its `Γ_v` contains the
free neighbors of `v`. With the maximality invariant above, these neighbors
can be found without storing `Γ_v`: for every `l ∈ L_v`, a valid free
`cand[l]` is the clique's only possible free neighbor. The code snapshots
the matched edges in increasing order of their smaller endpoint and visits
`L_u` and `L_v` for each edge `{u,v}` with `u < v`. It keeps up to two
distinct free candidates on each side in incidence order. If the first
choices coincide, it prefers `u`'s second choice over `v`'s. When it
finds a path `(x,u,v,y)` with `x ≠ y`, it replaces `{u,v}` by `{x,u}` and
`{v,y}`. One sweep suffices because augmentations only consume free
vertices, and each newly inserted edge has an endpoint that was free in the
maximal matching. The result has no augmenting path of length 1 or 3.

The repaired matching remains fixed throughout the following packing call.
This is needed both for Figure 7 and for the caches below.

## Figure 7: sorted terminal lists without repeated sorting

For each orientation `(y,z)` of each matched edge, Figure 7 puts a terminal
record in `β_l` for every `l ∈ L_y`, keyed by `w(z)`. Once the first repair
and early-exit checks have finished, the code sorts all vertex IDs by
`(w(v), v)` **once**, if packing is needed. In each packing round it
scans the vertices `z` in that order. If `z` is matched to `y`, it appends
`z` to every `β_l` with `l ∈ L_y`. The partner `y` remains available as
`mate[z]` while packing runs. Thus each list is already in key order even
though `M` changes between rounds.

The flat `β` storage, activity marks, and orientation witnesses are allocated
only if the first round reaches packing. After cardinality repair, each
clique has zero or one free vertex. Therefore
its record count is exactly `|C_l|` minus one if it has a valid free
`cand[l]`. These counts give boundaries in one flat `β` array; the sorted
vertex scan fills it. No individual list is sorted, and no extra counting
scan of the incidences is needed. Rebuilding `β` takes `O(n + σ(C))` per
round.

Figure 7 removes records when a matching edge becomes inactive. The code
instead keeps one cursor and a refillable cache of the first three active
terminal IDs for each `β_l`. A query drops inactive cached entries and
advances the cursor until it has three active entries or reaches the list's
end. Each record passes the cursor at most once in a packing call. The query
skips records belonging to its center edge. That edge contributes at most
two records to any one list, so three active entries suffice to find the
first eligible one. This preserves Figure 7's terminal choice without
deletion links or per-clique allocations. Each clique's state is five
32-bit integers (`next`, `end`, and three terminal IDs) stored contiguously
with the other clique states. A flat record stores one terminal ID as a
32-bit integer in plain mode or a packed value in encoded mode.

For an active orientation `(u,v)`, the code scans `L_u` to find the heaviest
active free `cand[l]` as source `S(u)`. It scans `L_v` and the `β` caches to
find the lightest eligible terminal `z` and includes the direct terminal
`(u,v)`. Equal weights favor the smaller vertex ID for both choices. The
path is `(S(u),u,v)` when `z = v`, or `(S(u),u,v,y,z)` when
`y = mate[z]` is on another matched edge. Its gain is `w(S(u)) − w(z)`.

An activity array marks free sources and matching edges consumed by selected
paths. At each threshold, edges are scanned by increasing smaller endpoint.
The orientation starting at that endpoint comes first. Selected paths are
recorded and their sources and matched edges are deactivated immediately.
`mate` is changed only after the complete packing call, when those
vertex-disjoint paths are flipped.

### Reusing source and terminal queries

The initial `g_max` scan in Figure 7 saves the best source and terminal IDs
for each edge orientation. It does not store a separate gain: the gain is
obtained from their weights. While `M` is fixed, active choices only
disappear. If both saved witnesses remain active, they are still optimal.
If one becomes inactive, the old gain is an upper bound on any replacement.
An orientation whose upper bound is below the current threshold is skipped;
otherwise the code recomputes whichever witness has become inactive and
updates it. A missing source or nonpositive initial gain cannot become
positive later in that packing call. The witnesses are two 32-bit vertex
IDs per orientation, indexed by the center's first vertex.

### Thresholds and the gain bound

Figure 7 examines thresholds `g_max / 2^i` down to `i = ceil(log_2 n)`.
For integer gains the code uses the equivalent test against
`t_i = ceil(g_max / 2^i)`. A vertex-disjoint family of increasing paths
uses a different free source and a different matching edge for each path.
It therefore has at most `a = min(f, μ)` paths; the paper uses the weaker
bound `n/3`.

After finishing a threshold level, let `g(B)` be the gain already selected.
The code may stop the levels when

```text
3 * a * (t_i - 1) <= g(B).
```

Every unconsidered positive integer gain is then at most `t_i - 1`, so the
total gain omitted from the comparison family is at most `a(t_i - 1)`, at
most `g(B)/3`. The paper's charging argument for the other paths is
unchanged. Thus its `19/3` packing bound, used in the Figure 8 round count,
still holds. The sum `g(B)` and the threshold stopping test use unsigned
128-bit arithmetic.

The first threshold yields `g(B) ≥ g_max`. At the paper's final level,
`t_i - 1 < g_max/n`, and `3a ≤ n` because `n = f + 2μ`. The stopping
test therefore succeeds by that level in bounded mode. In each extra
round of `until_no_improvement` after the bounded prefix, the code continues
down to threshold 1, so large remaining gains are considered before small
ones.

## Reading the cover in encoded mode

The matching, path choices, and round policy above are the same in both
modes. The difference is how the code builds `L_v` before the first round.
Plain mode starts with the decoded cover `C`. Encoded mode starts with the
CCF bytes. Its first scan pairs free vertices for Figure 5, counts the
incidences of each vertex, and saves each clique's size in a vbyte stream.
If any vertex remains free, the second scan visits clique labels in
descending order and fills each vertex's part of the packed incidence
array backwards, leaving its labels in increasing order. The CCF bytes
are released after the last scan. Later rounds use `L_v`, `cand`, and the
saved clique sizes.

For Figure 7, encoded mode stores the terminal IDs of all `β_l` lists in
one packed bit array. It uses the saved clique sizes and valid `cand[l]`
entries to set each list's boundaries. It zeroes the live bytes of that
array before every rebuild because writing packed records uses bitwise OR;
bits from the previous round would corrupt new terminal IDs. The same cursor
and three-terminal cache described above serve each list. After a packed
family of paths is flipped, the selected path records supply the released
terminal IDs for the next cardinality repair; no second terminal list is kept
for that step.

If Figure 5 matches every vertex, encoded mode can release the CCF bytes
and return after its first scan with `all_vertices_matched`, one attempted
round, and zero flips. Plain mode finishes filling `L_v` and performs its
first repair sweep before reporting the same result and counts. Both modes
use the same CLI keys, weight preparation, result field, and `mvm_execution`
fields. Encoded mode includes its scans in `algorithm` time.

Both modes require at most 2,147,483,647 incidences for the signed terminal
positions. Plain MVM also uses signed incidence positions. Its vertex and
clique counts must be below this value to leave room for boundary entries.
[Input size limits](representation.md#input-size-limits)

## Exits, result, and measured work

After repairing cardinality, the code can return before building `β` if
every vertex is matched. It can also return when the heaviest free vertex
is no heavier than the lightest matched vertex: no increasing path can then
have positive gain. The `g_max` scan can likewise return when it finds no
positive path. In each case, cardinality has already been repaired, so
Lemma 4.8 gives the `3/2` certificate. Reaching the bounded round limit
gives `(3/2 + ε)`.

The JSON report records the exact decimal `matched_vertex_weight_sum`,
the configured epsilon and round limit, `rounds_attempted`,
`rounds_with_flip`, `stop_reason`, and `early_exit`. An attempted round can
end with no flip, so the two counts differ in that case. The possible stop
reasons are `all_vertices_matched`, `weight_bound`, `no_positive_gain`, and
`round_limit`. `early_exit` is true when one of the first three reasons
stops the run before `T` rounds have flipped paths. An extended run that
continues past `T` has `early_exit = false`, even when its final reason is
`no_positive_gain`. The [report guide](reporting.md) defines all timing and
memory fields.

CCF loading is outside the algorithm timer in both modes. Plain mode also
decodes the full cover before that timer. Building `L_v`, the one-time weight
sort when needed, cardinality repairs, `β` builds, and packing are inside
it; encoded mode's two CCF scans are inside it as well. Weight generation
or file reading is reported separately. The reported peak RSS is a process
high-water mark: it may include the CCF and packed lists at one instant,
then the packed lists and later round state at another. It is not the sum
of their final sizes. For fixed `ε`, bounded mode takes
`O(n log n + σ(C) log n)` time and `O(n + σ(C))` auxiliary space. The
additional rounds in `until_no_improvement` have no comparable round bound.
