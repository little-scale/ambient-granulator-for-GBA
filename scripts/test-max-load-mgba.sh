#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
seconds="${1:-60}"

if [[ ! "$seconds" =~ ^[0-9]+$ ]] || (( seconds < 2 )); then
    printf 'Usage: %s [soak-seconds >= 2]\n' "$0" >&2
    exit 1
fi

"$project_dir/scripts/build-max-load.sh"
MGBATEST_SOAK_SECONDS="$seconds" \
    "$project_dir/scripts/test-mgba.sh" \
    ambient-granulator-for-gba-max-load.gba
