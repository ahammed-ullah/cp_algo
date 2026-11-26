#!/bin/bash
set -euo pipefail

algorithm=${1:?Usage: submit.sh <matching|bfs|dfs|wvc|mvm> [config.toml]}
config=${2:-}
repetitions=5
mkdir -p logs

while IFS= read -r graph || [[ -n "$graph" ]]; do
    graph=${graph%$'\r'}
    [[ -z "$graph" || "$graph" == \#* ]] && continue
    for ((run=1; run<=repetitions; ++run)); do
        if [[ -n "$config" ]]; then
            sbatch --job-name="${algorithm}-${graph}-${run}" \
                --output="logs/${algorithm}-${graph}-${run}-%j.out" \
                run.sh "$algorithm" "$graph" "$config"
        else
            sbatch --job-name="${algorithm}-${graph}-${run}" \
                --output="logs/${algorithm}-${graph}-${run}-%j.out" \
                run.sh "$algorithm" "$graph"
        fi
    done
done < graphs.txt
