#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
image="ambient-granulator-gba-dev:20260610"
target="ambient-granulator-for-gba-max-load"

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
        make BUILD=build-max-load \
            TARGET=ambient-granulator-for-gba-max-load \
            MAX_LOAD=1 MAX_LOAD_LPF_ONLY=1
        gbafix ambient-granulator-for-gba-max-load.gba \
            -p -tAMBGRANULAR -cAGRN -m00
        python3 tools/check_rom.py --require-power-of-two \
            ambient-granulator-for-gba-max-load.gba
        "$DEVKITARM/bin/arm-none-eabi-size" \
            ambient-granulator-for-gba-max-load.elf
    '

printf 'Built maximum-load profile: %s/%s.gba\n' "$project_dir" "$target"
