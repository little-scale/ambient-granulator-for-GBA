#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
image="ambient-granulator-gba-dev:20260610"

if ! docker image inspect "$image" >/dev/null 2>&1; then
    "$project_dir/scripts/setup.sh"
fi

docker run --rm \
    --user "$(id -u):$(id -g)" \
    --env HOME=/tmp \
    --env PYTHONDONTWRITEBYTECODE=1 \
    --volume "$project_dir:/work" \
    --workdir /work \
    "$image" \
    bash -lc '
        set -euo pipefail
        python3 tests/bank_test.py
        mkdir -p build/host-tests
        gcc -std=c11 -O2 -Wall -Wextra -Werror -Isource \
            -DSAMPLE_BANK_NO_EMBEDDED \
            tests/sample_bank_test.c source/sample_bank.c \
            -o build/host-tests/sample_bank_test
        build/host-tests/sample_bank_test
        for test in timing position controls audio_handoff; do
            gcc -std=c11 -O2 -Wall -Wextra -Werror -Isource \
                "tests/${test}_test.c" -o "build/host-tests/${test}_test"
            "build/host-tests/${test}_test"
        done
        gcc -std=c11 -O2 -Wall -Wextra -Werror -Isource \
            tests/parameters_test.c source/parameters.c source/text_format.c \
            -o build/host-tests/parameters_test
        build/host-tests/parameters_test
        gcc -std=c11 -O2 -Wall -Wextra -Werror -Isource \
            tests/dsp_test.c source/dsp.c source/parameters.c source/text_format.c \
            -o build/host-tests/dsp_test
        build/host-tests/dsp_test
        gcc -std=c11 -O2 -Wall -Wextra -Werror -Isource \
            tests/click_test.c source/dsp.c source/parameters.c source/text_format.c \
            -o build/host-tests/click_test
        build/host-tests/click_test
        gcc -std=c11 -O2 -Wall -Wextra -Werror -Isource \
            -DSAMPLE_BANK_NO_EMBEDDED \
            tests/sample_click_test.c source/dsp.c source/parameters.c \
            source/text_format.c source/sample_bank.c \
            -o build/host-tests/sample_click_test
        build/host-tests/sample_click_test
        gcc -std=c11 -O2 -Wall -Wextra -Werror -Isource \
            tests/offline_renderer.c source/dsp.c source/parameters.c source/text_format.c \
            -o build/host-tests/offline_renderer
        build/host-tests/offline_renderer
    '
