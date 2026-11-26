# Slurm runs

The scripts follow `cc_con`'s `run.sh`/`submit.sh` pattern. Build on the
cluster and place `<graph>.ccf` files in
`$RCAC_SCRATCH/fall2026/format_ccf`. `graphs.txt` contains the 48 graph names
used in the paper. The artifact provides the experiment reports and dataset
metadata. [*Concise Clique Covers for Parsimonious Graph Algorithms*](https://arxiv.org/abs/2604.28096)

```sh
make -j
./submit.sh matching
./submit.sh dfs config/encoded.toml
```

`submit.sh` submits five independent jobs per graph. `run.sh` starts one
`cp_algo` process in each job. Its JSON report contains both
`timing_seconds.algorithm` and `memory.process_peak_rss_bytes`, so no
separate memory collection file is required. The report also records
`slurm_job_id` for matching it to the Slurm log.

The memory field is the kernel-reported high-water resident set size of the
process, read immediately after the algorithm timer. It includes input
loading, plain cover decoding when selected, weight preparation, and the
algorithm. The measurement is outside `timing_seconds.algorithm`.

To check Slurm's separate memory statistic for a completed job, use its
batch step:

```sh
sacct -j JOB_ID.batch --format=JobID,State,ExitCode,MaxRSS
```

RCAC [documents this `sacct` field](https://docs.rcac.purdue.edu/lifesciences/guides/running-bioinformatics/)
as peak memory. [Slurm defines](https://slurm.schedmd.com/sacct.html) it for
a job step; it need not equal the process RSS field, since accounting plugins
can count memory differently. The site's accounting configuration is not
known from the repository, so the JSON process field is the primary memory
measurement and Slurm `MaxRSS` is a cross-check.

Shared filesystem traffic may affect CCF load time. The timed algorithm
starts from the prepared cover state described in [reporting.md](reporting.md).
Adjust account, partition, time, and memory directives for another cluster.
