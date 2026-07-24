#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
image="ambient-granulator-gba-dev:20260610"
artifact_dir="$project_dir/build/mgba-fifo-test"

"$project_dir/scripts/build-fifo-test.sh"
mkdir -p "$artifact_dir"

docker run --rm \
    --user "$(id -u):$(id -g)" \
    --env HOME=/tmp \
    --volume "$project_dir:/work" \
    --workdir /work \
    "$image" \
    bash -lc '
        set -euo pipefail
        export DISPLAY=:99
        export SDL_AUDIODRIVER=disk
        export SDL_DISKAUDIOFILE=/tmp/audio.raw
        export LIBGL_ALWAYS_SOFTWARE=1

        Xvfb :99 -screen 0 640x480x24 >/tmp/xvfb.log 2>&1 &
        xvfb_pid=$!
        emulator_pid=""
        trap '\''
            cp /tmp/audio.raw build/mgba-fifo-test/audio.raw 2>/dev/null || true
            cp /tmp/emulator.log build/mgba-fifo-test/emulator.log 2>/dev/null || true
            if [ -n "$emulator_pid" ]; then kill "$emulator_pid" 2>/dev/null || true; fi
            kill "$xvfb_pid" 2>/dev/null || true
        '\'' EXIT
        sleep 1

        /usr/games/mgba -2 ambient-granulator-for-gba-fifo-test.gba \
            >/tmp/emulator.log 2>&1 &
        emulator_pid=$!
        for _ in $(seq 1 60); do
            if xdotool search --onlyvisible --name "mGBA" >/dev/null 2>&1; then
                break
            fi
            sleep 0.1
        done
        sleep 4
        kill "$emulator_pid" 2>/dev/null || true
        wait "$emulator_pid" 2>/dev/null || true
        emulator_pid=""

        python3 - <<'\''PY'\''
import math
import struct
from pathlib import Path

raw = Path("/tmp/audio.raw").read_bytes()
if len(raw) < 262144 or len(raw) % 4:
    raise SystemExit("FIFO diagnostic capture is missing or malformed")
samples = struct.unpack(f"<{len(raw) // 2}h", raw)
left = samples[0::2]
right = samples[1::2]
frames = min(len(left) // 2, 131072)
left = left[-frames:]
right = right[-frames:]
maximum_jump_left = max(abs(b - a) for a, b in zip(left, left[1:]))
maximum_jump_right = max(abs(b - a) for a, b in zip(right, right[1:]))
left_rms = math.sqrt(sum(value * value for value in left) / len(left))
right_rms = math.sqrt(sum(value * value for value in right) / len(right))
if left_rms < 1000 or right_rms < 1000:
    raise SystemExit("FIFO diagnostic capture is unexpectedly quiet")
if maximum_jump_left > 512 or maximum_jump_right > 512:
    raise SystemExit(
        "FIFO block handoff dropped or repeated samples: "
        f"max jump L={maximum_jump_left} R={maximum_jump_right}"
    )
print(
    "FIFO continuity passed: "
    f"{frames} frames, RMS L={left_rms:.1f} R={right_rms:.1f}, "
    f"max jump L={maximum_jump_left} R={maximum_jump_right}"
)
PY
    '

printf 'mGBA FIFO continuity regression passed. Capture: %s/audio.raw\n' \
    "$artifact_dir"
