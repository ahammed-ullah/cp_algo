# Matching

Figures 5 and 6 compute a maximal matching `M'` and then a 3/2-approximate
matching `M`. The plain and encoded implementations report the matching
size `|M|`.
[*Concise Clique Covers for Parsimonious Graph Algorithms*](https://arxiv.org/abs/2604.28096) · [Plain implementation](../src/plain/matching.cpp) · [Encoded implementation](../src/encoded/matching.cpp)

## Figure 5: maximal matching

The paper forms the unmatched set `U` of each clique and pairs its vertices.
The code visits clique members in their stored order. `pending` is the first
unpaired vertex of `U`; the next unmatched member is paired with it and
`pending` is cleared. `mate[v] = -1` corresponds to the paper's `I_v = 0`.

After a clique is processed, at most one of its members remains unmatched.
Since every graph edge belongs to a clique, the resulting matching `M'` is
maximal. With `matching_variant = "maximal"`, the algorithm returns `|M'|` at
this point.

## Figure 6: 3/2-approximate matching

Figure 6 first computes `M'`, then constructs `Γ_v` from neighbors of `v`
unmatched in `M'`. Immediately after pairing the unmatched members of
`C_l`, if `pending = x` remains, the code records `x` for every other
`v ∈ C_l`. It also saves each edge of the paper's `M'` in `M0`.

Plain mode builds `M0` and `gamma` in one cover scan. Encoded mode uses two
scans of the CCF bytes already in memory. The first makes the same initial
matching and counts the bytes needed by each `gamma[v]`. The second repeats
the initial pairing to rebuild `mate`, saves `M0`, and fills one flat
vbyte array. One 32-bit boundary per vertex locates its list. A clique
with a remaining free member is read again to visit all members without
retaining a decoded clique. These scans and the construction of `gamma`
are included in algorithm time. Plain and encoded modes produce the same
initial `mate` and `M0`.

An `x` appended early might be matched in a later clique. Such entries are
skipped by the `mate[x] == -1` test when `gamma[v]` is read. Conversely, if
`x` is unmatched in the completed `M'`, then it was the unique unmatched
member whenever a clique containing `x` finished its pairing. Therefore
every neighbor of `v` unmatched in `M'` appears in `gamma[v]`, as required
by Figure 6. Repeated entries are allowed.

The second part of the code scans each saved edge `{u,v} ∈ M0` once. It
looks for currently unmatched `x ∈ gamma[u]` and `y ∈ gamma[v]` with
`x ≠ y`, then replaces `{u,v}` by `{x,u}` and `{v,y}`. It retains the first
two distinct free entries of each list. If the first choices coincide, the
code prefers `u`'s second choice over `v`'s second choice.
Each replacement raises `|M|` by one.

The remaining free vertices are a subset of those free in `M'`. Thus an
original edge that cannot be augmented when examined cannot acquire free
neighbors later. A newly inserted matching edge has one endpoint free in
`M'`, which cannot have a free neighbor because `M'` was maximal. This is
the one-sweep argument behind Lemma 4.5: the final matching has no
augmenting path of length one or three.

In `plain` mode, `gamma[v]` stores vertex IDs as integers. In `encoded`
mode, its flat array stores the same IDs as vbytes. Both modes visit clique
labels and members in stored order and make the same matching choices. The
work and list length are `O(n + σ(C))`, where `σ(C) = Σ_l |C_l|`.

Encoded 3/2 matching accepts at most 858,993,459 incidences so that the
candidate array fits its 32-bit byte offsets even with five bytes per entry.
The encoded maximal matching pass streams the cliques directly.
[Input size limits](representation.md#input-size-limits)
