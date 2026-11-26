# Reporting and timing

Every successful command writes one JSON report in the current directory.
For example, a plain matching run on `email-enron.ccf` writes
`matching-email-enron-plain.json`. If the name is taken, the next report uses
`-2`, `-3`, and so on. A claim directory reserves each name before the
algorithm starts, so independent Slurm jobs can write reports concurrently.
The final `.json` is published only after the complete run succeeds.

The report identifies the algorithm, mode, config path, CCF path, graph
fingerprint, stored CCF checksum, `n`, `m`, `|C|`, `sigma(C)`, CCF file bytes,
and optional Slurm job ID. Its one scalar result is `matching_size`,
`connected_components`, `selected_weight_sum`, or
`matched_vertex_weight_sum`. Both weighted sums are exact decimal strings
because they can exceed 64 bits. The `memory` section contains
`process_peak_rss_bytes` for that same invocation.

An `mvm` report also records `mvm_epsilon`, `mvm_round_limit`, and `mvm_mode`
under `parameters`. Its `mvm_execution` section has `rounds_attempted`,
`rounds_with_flip`, `stop_reason`, and `early_exit`. An attempted final round
can find no positive increasing path, so it may add no flip. `early_exit`
means that a certified stop happened before the configured bounded prefix
of flipping rounds was completed. Reaching `round_limit` ends bounded mode
with the `(3/2 + ε)` guarantee. The other stop reasons describe a stronger
`3/2` certificate; see the [MVM guide](mvm.md).

## Timed phases

The command uses a monotonic wall clock and records these separate times:

1. `ccf_load`: read the CCF bytes and check the header and offsets needed to
   access records. It does not decode every record or recompute the checksum.
2. `cover_decode` (plain mode): decode all cliques into `C` and release the CCF
   byte buffer.
3. `weight_preparation` (MVM and WVC): generate default weights or read the
   weight file.
4. `algorithm`: run the chosen paper algorithm from the prepared cover state.
   This includes building `L[v]` for BFS/DFS, building `Gamma[v]` for the
   3/2 cardinality-matching algorithm, copying WVC weights into residual
   weights, and MVM's incidence construction, one-time weight sort, and
   matching rounds. Encoded MVM builds its packed lists from two CCF scans
   in this phase, then releases the CCF bytes. If the first scan matches
   every vertex, it releases the bytes and returns after that scan.

Only phase 4 is the algorithm time. Scalar conversion and JSON publication
take place after it. The load and preparation times remain in each report,
even when shared filesystem traffic makes them unsuitable for comparison.

The program reads `getrusage(RUSAGE_SELF)` after the algorithm timer stops.
`process_peak_rss_bytes` is the kernel-reported high-water mark from process
startup through that sample. It includes input loading and preparation. Plain
decoding can briefly hold both the CCF bytes and `C`. Linux reports
`ru_maxrss` in KiB, which the program converts to bytes. macOS reports bytes.
[Linux getrusage manual](https://man7.org/linux/man-pages/man2/getrusage.2.html)

Slurm's batch-step `MaxRSS` is available separately through `sacct`. Its
collection method depends on the site configuration, so it is a cross-check
rather than the source of the JSON memory field.

Each command runs once. `submit.sh` schedules five independent commands per
graph, and the JSON file and Slurm job ID identify each run. For a comparison
that avoids contested I/O, use `algorithm` time and record the size of each
input representation. The load time can be analyzed separately when the
filesystem is controlled.
