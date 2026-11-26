# BFS and DFS

Figures 9 and 10 describe BFS and DFS from a clique cover. Both start by
forming the incidence lists `L_v = {l : v ∈ C_l}`.
[*Concise Clique Covers for Parsimonious Graph Algorithms*](https://arxiv.org/abs/2604.28096)

The four implementations build those lists from the input cover in increasing
label order.
[`src/plain/bfs.cpp`](../src/plain/bfs.cpp),
[`src/encoded/bfs.cpp`](../src/encoded/bfs.cpp),
[`src/plain/dfs.cpp`](../src/plain/dfs.cpp), and
[`src/encoded/dfs.cpp`](../src/encoded/dfs.cpp).

The figures describe parent pointers. These runs perform the same vertex
discoveries but count the forest's roots instead of saving `π(v)`: the
reported number is the number of connected components. To recover a forest,
save `π(u) = v` when either traversal marks `u` discovered.

## Figure 9: BFS

The paper's `J_l` marks a clique after its first scan. In the code it is
`used[q]`, where `q` is a clique label. When `v` leaves the FIFO queue, the
algorithm examines `L[v]` (the paper's `L_v`). For each unused label, it
marks `used[q]` and visits `C_q`; every undiscovered member enters the queue.
The newly discovered members are all adjacent to `v`, so this is the BFS
step of Figure 9. Each clique
is scanned at most once after `L_v` is built.

Figure 9 starts from one root. The code repeats its queue traversal from
each still-undiscovered vertex in increasing ID order, retaining the same
discovery and `used` arrays. This is the paper's extension to a BFS forest;
an isolated vertex contributes one root.

## Figure 10: DFS

Figure 10 keeps a shared cursor `a_l` for each clique. In the code, `a[q]`
is that cursor. When `DFS-Visit(v)` examines `l ∈ L_v`, it advances `a_l`
until it finds an undiscovered member `u`, visits `u` immediately, then
resumes the same clique after returning.
The code uses an explicit `stack` of frames in place of recursive calls.
Each frame remembers its current vertex and position in `L_v`; the clique
cursor remains shared across all frames.

For example, a child `u` can itself encounter `C_l` and advance `a_l`
before its parent resumes that clique. This is why the code does not push
all unseen members of a clique together. Positions before `a_l` have
already been inspected and their vertices discovered, matching the
invariant in Lemma 4.13. The outer loop of Figure 10 already visits every
undiscovered root, so it produces a DFS forest directly.

In `plain` mode, `L[v]` contains integer labels and `a[q]` is an index into
the decoded `C_q`. In `encoded` mode, all `L[v]` share one bit-packed array.
The labels for a vertex are consecutive, in increasing clique order, and a
32-bit offset locates that vertex's part. Two scans of the CCF bytes already
in memory build this array: one counts incidences; the other fills the array
from the ends of the vertex ranges while visiting cliques in reverse order.
Both scans belong to the encoded algorithm time. A fixed label width is
chosen from the number of cliques, so each incidence has that same width.

Encoded DFS keeps, for each clique, the byte position of its next member
and the previous vertex needed to decode the next gap. It does not keep a
decoded neighborhood or restart a clique when returning from a child.
Both modes have `O(n + σ(C))` work: every `L_v` is read once and each
clique's members advance once during the traversal. The second incidence
building scan increases the encoded mode's constant cost while reducing
the memory used by its cover index.
