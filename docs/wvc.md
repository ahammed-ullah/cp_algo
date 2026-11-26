# Weighted vertex cover

`Approximate-WVC` in Figure 18 gives a 2-approximation for minimum weighted
vertex cover. Figure 17 gives its primal and dual linear programs. Lemma B.2
proves the guarantee.
[*Concise Clique Covers for Parsimonious Graph Algorithms*](https://arxiv.org/abs/2604.28096)

The plain and encoded implementations follow Figure 18.
[`src/plain/wvc.cpp`](../src/plain/wvc.cpp) and
[`src/encoded/wvc.cpp`](../src/encoded/wvc.cpp)

The paper starts with residual weight `r_v = w_v`. For each clique `C_l`,
it sets `x = ⊥`, then scans the members whose residual is positive. The
code calls `x` `pending`. When it encounters another such member `v`, it
performs the paper's update:

```text
δ = min(r_x, r_v)
r_x = r_x − δ
r_v = r_v − δ
```

This raises the dual value of edge `{x,v}` by `δ`; that edge exists because
both vertices belong to `C_l`. At least one residual becomes zero. The code
keeps the endpoint with positive residual as `pending`, or clears it when
both become zero. Thus, after each clique, at most one of its vertices has
positive residual. Every edge in that clique has a tight endpoint, and
the tight vertices form the cover `S` of Figure 18.

The program reports `w(S)` directly. When a positive residual first reaches
zero, it adds the vertex's original weight to `cover_weight`. A tight vertex
is skipped on later clique visits, so its weight is added once. The code
does not need to retain `S` or the individual edge dual values to compute
this sum. Lemma B.2 shows `w(S) ≤ 2w(S*)` by dual feasibility and the fact
that each dual edge contributes to at most two tight vertices.

The two modes execute the same residual updates in clique-label and member
order. `plain` reads decoded cliques; `encoded` reads each CCF clique
through a cursor. Neither uses `L_v`. Weights and residuals use unsigned
64-bit integers. `cover_weight` uses unsigned 128-bit arithmetic because
the default weights can total more than 64 bits, and the JSON report writes
the exact decimal value. The run takes `O(n + σ(C))` time and `O(n)`
additional space.

Encoded WVC already holds no cover-derived index: besides the CCF bytes,
it keeps only the cursor for the current clique. Its weight and residual
arrays are the same as in plain mode.
