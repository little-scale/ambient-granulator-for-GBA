#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
image="ambient-granulator-gba-dev:20260610"
target="ambient-granulator-for-gba-fifo-test"

if ! docker image inspect "$image" >/dev/null 2>&1; then
    "$project_dir/scripts/setup.sh"
fi

docker run --rm \
    --user "$(id -u):$(id -g)" \
    --env HOME=/tmp \
    --volume "$project_dir:/work" \
    --workdir /work \
    "$image" \
    bash -lc '
        set -euo pipefail
        python3 tools/build_sample_bank.py \
            --input samples \
            --output assets/sample_bank.bin \
            --capacity 8388608
        make BUILD=build-fifo-test \
            TARGET=ambient-granulator-for-gba-fifo-test \
            FIFO_TEST=1
        gbafix ambient-granulator-for-gba-fifo-test.gba \
            -p -tAMBGRANULAR -cAGRN -m00
        python3 tools/check_rom.py --require-power-of-two \
            ambient-granulator-for-gba-fifo-test.gba
    '

printf 'Built FIFO continuity profile: %s/%s.gba\n' "$project_dir" "$target"
