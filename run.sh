#!/bin/bash
#SBATCH -A csit
#SBATCH -p cpu
#SBATCH --nodes=1
#SBATCH --time=4:00:00
#SBATCH --mem-per-cpu=128GB

set -euo pipefail

algorithm=${1:?Usage: sbatch run.sh <algorithm> <graph-name> [config.toml]}
graph=${2:?Usage: sbatch run.sh <algorithm> <graph-name> [config.toml]}
cover_directory="${RCAC_SCRATCH:?Set RCAC_SCRATCH}/fall2026/format_ccf"

cd "$SLURM_SUBMIT_DIR"

if [[ $# -eq 3 ]]; then
    exec ./bin/cp_algo "$algorithm" "${cover_directory}/${graph}.ccf" --config "$3"
else
    exec ./bin/cp_algo "$algorithm" "${cover_directory}/${graph}.ccf"
fi
