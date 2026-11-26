# The cover in memory

The paper gives the algorithms a clique cover
`C = (C[0], ..., C[p-1])`, with `sigma(C) = sum_ell |C[ell]|`.
`cc_con` writes this final cover to CCF after any requested
assignment-minimal postprocessing. Clique label `ell` is its record number.
The vertices within a record are in increasing ID order.
[*Concise Clique Covers for Parsimonious Graph Algorithms*](https://arxiv.org/abs/2604.28096)

The paper's algorithms do not require CCF or any other encoding. `cp_algo`
therefore offers two ways to supply the same ordered cover to their loops.

## Plain mode

Plain mode is the default. Before timing an algorithm, the program decodes
the entire file into `vector<vector<int>> C` and releases the CCF byte buffer.
The measured call starts with this `C`. BFS, DFS, and vertex-weighted matching
then construct the paper's incidence lists `L[v]`; cardinality matching
constructs its free-neighbor lists `Gamma[v]` when needed. Weighted vertex
cover uses `C` without either list. Vertex-weighted matching also builds
sorted terminal lists `beta[l]` during its rounds; their construction and use
belong to algorithm time.

The file read and cover decoding have separate reported times. Neither is
included in `timing_seconds.algorithm`.

## Encoded mode

Encoded mode starts the measured call with the CCF bytes in memory. A cursor
advances through one clique record without keeping all cliques decoded.
The cursor returns members in increasing vertex order. Each vertex's
incidence list stores its clique labels in increasing order.

Vertex-weighted matching starts with an in-memory CCF scan that
runs the initial maximal matching, counts incidences for `L[v]`, and stores
each clique size as a vbyte. If this matches every vertex, it releases the
CCF bytes and returns. Otherwise, a second scan fills the packed `L[v]`
labels. It then releases the CCF bytes. Later rounds read these labels and
the saved sizes. Their terminal lists `beta[l]` use packed vertex IDs.
[MVM guide for Figures 5–8](mvm.md)

BFS and DFS construct the paper's `L[v]` in two scans of the CCF bytes already
in memory. One array holds every incidence label in a fixed number of bits;
one 32-bit offset per vertex locates its part of that array. The first scan
counts labels and the second fills each part in reverse clique order, leaving
the labels in increasing order for the traversal. DFS also keeps the current
byte position and previous vertex for each clique, so it can resume that
clique without storing its decoded members.

The 3/2 cardinality-matching algorithm also uses two in-memory cover scans. It stores
`Gamma[v]` in one vbyte array with one 32-bit boundary per vertex. Its entries
remain **absolute candidate vertex IDs** in clique order; a candidate that
becomes matched later is still skipped when the list is read. Maximal matching
and weighted vertex cover need no incidence or candidate lists and continue
to stream one CCF clique at a time.

All incidence and candidate construction belongs to the encoded algorithm
time. The encoded algorithms also retain their ordinary matching, traversal,
or weight arrays. The CCF bytes plus cover-derived structures are therefore
separate from the whole-process peak RSS reported for a run.

## Cover-related space

Write `S` for the CCF file bytes, `p` for the number of cliques, and
`sigma(C)` for the total number of clique members. If `b` bits suffice for a
clique label, encoded BFS holds approximately
`S + 4(n+1) + ceil(sigma(C)*b/8) + ceil(p/8)` bytes of cover-related data:
the CCF, incidence offsets and labels, and its used-clique bits. Encoded DFS
replaces the last term with `8p` bytes for its shared clique cursors. These
counts exclude the queue, DFS stack, and visited vertices. Maximal matching
and weighted vertex cover retain just the CCF and one current-clique cursor
on this side. For 3/2 matching the cover-related data is `S`, one 32-bit
boundary per vertex, and the vbyte candidate array; its size depends on the
cover and the initial matching. Encoded MVM first holds `S`, packed incidence
labels and their 32-bit boundaries, and the vbyte clique-size stream. After
releasing `S`, it builds packed `beta` records, their boundaries and cursors,
and the Figure 7 matching state. The terminal records can approach
`sigma(C)` in number. The sizes of these parts depend on `n`, `p`, and the
cover structure as well as `S`; there is no fixed multiple of CCF bytes that
bounds the whole process's peak RSS. Released CCF pages may also remain
resident in the allocator's high-water mark.

## Input size limits

CCF stores 64-bit counts and offsets on disk. The reader limits the numbers
of vertices and cliques to 2,147,483,647 each because the algorithms use
signed 32-bit IDs. Some algorithm paths use narrower working limits.

| Algorithm and mode | Working limit |
| --- | --- |
| Encoded BFS and DFS | At most 4,294,967,295 incidences for packed clique lists. Encoded DFS also limits the CCF file to 4,294,967,295 bytes for its saved clique positions. |
| Encoded 3/2 cardinality matching | At most 858,993,459 incidences. The check allows up to five bytes per incidence for the candidate array. The vertex count must also be below 2,147,483,647 to leave room for an extra boundary entry. |
| MVM in either mode | At most 2,147,483,647 incidences so that terminal positions fit in signed 32-bit integers. Plain MVM also requires vertex and clique counts below 2,147,483,647 to leave room for extra boundary entries. |

Encoded BFS, DFS, and 3/2 cardinality matching check the listed incidence
limits. Encoded DFS also checks the file size. The caller must honor the
signed MVM limits. Encoded MVM's incidence check covers its unsigned packed
positions, while terminal positions use the smaller signed range.

Across the 48 evaluation covers, the largest cover has 75,703,657 incidences
and the largest CCF is 252,057,746 bytes.

## Input guarantees

`src/ccf.cpp` checks the CCF header and offset table required for record
access. It does not rescan every record or recompute the checksum while
loading. Validate the cover once with `cc_check cover` before experiment
runs; pass the graph specification as well when checking clique and edge
coverage. The algorithm loops assume that validated input, just as `cc_con`
construction loops assume their prepared graph.
