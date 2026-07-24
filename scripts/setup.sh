#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
image="ambient-granulator-gba-dev:20260610"

docker build --tag "$image" --file "$project_dir/tools/Dockerfile" "$project_dir"

printf 'GBA development image ready: %s\n' "$image"

