# Vertex weights and configuration

The MVM algorithm in Section 4.2 and WVC in Figure 18 use the same positive
integer vertex weights in the experiments.
[*Concise Clique Covers for Parsimonious Graph Algorithms*](https://arxiv.org/abs/2604.28096)

The default instance assigns one weight to each vertex `0,…,n−1`, in that
order. It uses the C++ standard
`std::mt19937_64` engine seeded with `19u`, and draws from `[1,n²]` by this
fixed rejection mapping:

```text
bound = n*n
threshold = (0 - bound) modulo 2^64, then modulo bound
repeat draw = engine() while draw < threshold
weight = 1 + (draw modulo bound)
```

The rule uses only specified engine output and integer arithmetic; it does
not depend on a library's `uniform_int_distribution` mapping. The empty graph
has no weights. Generated weights are prepared before the algorithm timer.

To use a fixed instance, set `weights_file = "weights.txt"` in a config.
The file contains exactly `n` positive decimal integers, one per nonempty
line, for vertices `0,…,n−1`. Blank lines and `#` comments are allowed.
Each value must fit unsigned 64-bit arithmetic. Relative paths are resolved
from the config file directory. File reading is included in
`timing_seconds.weight_preparation`, outside `timing_seconds.algorithm`.

Graph and clique labels remain `int` under the same bound as `cc_con`.
Individual weights and WVC residuals are unsigned 64-bit. Even with default
weights, `n × n²` may exceed 64 bits, so both the MVM matched-vertex sum and
the WVC selected-cover sum are accumulated in unsigned 128-bit arithmetic
and written to JSON as exact decimal **strings**. The implementation never
narrows either sum to 64 bits.
