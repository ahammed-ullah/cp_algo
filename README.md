# cp_algo

`cp_algo` implements cardinality matching, vertex-weighted matching, breadth-first
search, depth-first search, and weighted vertex cover on a clique cover. It reads
the final cliques from a `.ccf` file produced by `cc_con`.
[*Concise Clique Covers for Parsimonious Graph Algorithms*](https://arxiv.org/abs/2604.28096)

## Requirements and quick start

Build on Linux or macOS with a C++17 compiler, GNU Make, and support for
unsigned 128-bit arithmetic. Python 3 runs the integration tests.

```bash
make
make test
```

Create a cover of a small random intersection graph using the `cc_con` example.
The commands assume `cc_con` is a sibling directory, as in the paper artifact.
Run them from the `cp_algo` directory.

```bash
make -C ../cc_con
(
    cd ../cc_con
    mkdir -p build/example-output
    bin/cc_con bpa \
        --graph examples/graphs/hypergraph-k-uniform.toml \
        --run examples/runs/write-cover.toml
    bin/cc_check cover \
        --cover build/example-output/hypergraph-k-example.ccf \
        --graph examples/graphs/hypergraph-k-uniform.toml
)
cover=../cc_con/build/example-output/hypergraph-k-example.ccf
bin/cp_algo matching "$cover"
bin/cp_algo mvm "$cover" --config config/encoded.toml
```

For the SuiteSparse example, create the `coPapersCiteseer.toml` graph
specification described in `cc_con/examples/README.md`. Pass its path to both
`--graph` options above and keep `examples/runs/write-cover.toml` as the run
specification. The CCF will be `build/example-output/coPapersCiteseer.ccf` in
the `cc_con` directory.

## Commands and configuration

```text
cp_algo <matching|mvm|bfs|dfs|wvc> graph.ccf [--config config.toml]
```

Use root-level `key = value` lines. Include only the settings that apply to
the command you are running.

| Key | Commands | Values |
| --- | --- | --- |
| `cover_mode` | All | `"plain"` by default or `"encoded"` |
| `matching_variant` | `matching` | `"three_halves"` by default or `"maximal"` |
| `weights_file` | `mvm`, `wvc` | Quoted path, resolved from the config file's directory. The default is generated weights. |
| `mvm_epsilon` | `mvm` | Positive finite number. The default is `0.01`. |
| `mvm_mode` | `mvm` | `"bounded"` by default or `"until_no_improvement"` |

The repository includes `config/encoded.toml` for encoded runs and
`config/maximal.toml` for maximal cardinality matching.

`mvm` runs in either cover mode. Its bounded policy gives the paper's
`(3/2 + ε)` approximation; `until_no_improvement` continues to a `3/2`
certificate. [Vertex-weighted matching](docs/mvm.md) explains the two
policies and the correspondence between the paper and the code.

## Paper algorithms

The algorithm guides start with the paper's notation and loops, then explain
the few implementation choices needed to read a `.ccf` cover:

- [Matching](docs/matching.md): Figures 5 and 6. The default is the
  3/2-approximation; maximal matching is an option.
- [Vertex-weighted matching](docs/mvm.md): Figures 5–8. The default is a
  `(3/2 + 0.01)`-approximation.
- [BFS and DFS](docs/traversals.md): Figures 9 and 10. Both visit every
  component and return the number of components.
- [Weighted vertex cover](docs/wvc.md): Figure 18. It returns the sum of
  selected vertex weights. [Weights](docs/weights.md) specifies the input.

Each command writes one JSON report in the working directory and prints its
path. The report includes algorithm time, process peak resident memory, and
one scalar result. `matching` reports the matching size. `mvm` and `wvc`
report the sum of the matched or selected vertex weights. `bfs` and `dfs`
report the component count. [Reporting guide](docs/reporting.md)

The paper's cluster runs used five independent jobs per graph.
[Slurm run instructions](docs/slurm.md)

## Cover input

The decoded cover is a `vector<vector<int>> C`: `C[ell]` contains the vertices
of clique `ell` in increasing order. Vertex IDs and clique labels use `int`,
as in `cc_con`. The CCF file uses 64-bit positions and counts independently.
The two [cover modes](docs/representation.md) differ in whether the complete
`C` is decoded before the algorithm timer or individual CCF cliques are read
during it.

The CCF reader checks the header and record offsets needed to read the file.
Run `cc_con`'s `cc_check cover --cover graph.ccf --graph graph.cgf.toml`
before experiments to verify the file, its cliques, and edge coverage.
Algorithms rely on that validated cover; they do not repeat clique or index
checks in their main loops.

## Source layout

- `src/main.cpp`: CLI, input preparation, and timing boundaries.
- `src/ccf.cpp`: CCF input and one-clique cursor.
- `src/plain/`: plain-cover algorithms, including vertex-weighted matching.
- `src/encoded/`: encoded-cover implementations of all five algorithms.
- `src/weights.cpp`, `src/config.cpp`, `src/report.cpp`: inputs and reporting.
- `tests/integration.py`: end-to-end checks through the public command.
