#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
image="ambient-granulator-gba-dev:20260610"
rom="ambient-granulator-for-gba.gba"

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
        if [ "${1:-}" = clean ]; then
            make clean
            exit 0
        fi
        python3 tools/build_sample_bank.py \
            --input samples \
            --output assets/sample_bank.bin \
            --capacity 8388608
        make "$@"
        gbafix ambient-granulator-for-gba.gba \
            -p -tAMBGRANULAR -cAGRN -m00
        python3 tools/check_rom.py --require-power-of-two \
            ambient-granulator-for-gba.gba
        "$DEVKITARM/bin/arm-none-eabi-size" \
            ambient-granulator-for-gba.elf
    ' bash "$@"

if [[ "${1:-}" != clean ]]; then
    printf 'Built %s/%s\n' "$project_dir" "$rom"
fi
