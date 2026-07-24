#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
image="ambient-granulator-gba-dev:20260610"
artifact_dir="$project_dir/build/mgba-test"
test_rom="${1:-ambient-granulator-for-gba.gba}"
soak_seconds="${MGBATEST_SOAK_SECONDS:-2}"

if [[ "$test_rom" = /* || "$test_rom" == ../* || ! -f "$project_dir/$test_rom" ]]; then
    printf 'Test ROM must be a project-relative file: %s\n' "$test_rom" >&2
    exit 1
fi
if [[ ! "$soak_seconds" =~ ^[0-9]+$ ]] || (( soak_seconds < 2 )); then
    printf 'MGBATEST_SOAK_SECONDS must be an integer of at least 2\n' >&2
    exit 1
fi

"$project_dir/scripts/build.sh"
mkdir -p "$artifact_dir"
rm -f "$artifact_dir"/*.png "$artifact_dir"/*.log "$artifact_dir"/*.raw

docker run --rm \
    --user "$(id -u):$(id -g)" \
    --env HOME=/tmp \
    --env TEST_ROM="$test_rom" \
    --env SOAK_SECONDS="$soak_seconds" \
    --volume "$project_dir:/work" \
    --workdir /work \
    "$image" \
    bash -lc '
        set -euo pipefail
        export DISPLAY=:99
        export SDL_AUDIODRIVER=disk
        export SDL_DISKAUDIOFILE=/tmp/audio.raw
        export LIBGL_ALWAYS_SOFTWARE=1
        mkdir -p build/mgba-test

        Xvfb :99 -screen 0 640x480x24 >/tmp/xvfb.log 2>&1 &
        xvfb_pid=$!
        emulator_pid=""
        trap '\''
            cp /tmp/xvfb.log build/mgba-test/xvfb.log 2>/dev/null || true
            cp /tmp/emulator.log build/mgba-test/emulator.log 2>/dev/null || true
            cp /tmp/audio.raw build/mgba-test/audio.raw 2>/dev/null || true
            if [ -n "$emulator_pid" ]; then kill "$emulator_pid" 2>/dev/null || true; fi
            kill "$xvfb_pid" 2>/dev/null || true
        '\'' EXIT
        sleep 1

        /usr/games/mgba -2 "$TEST_ROM" \
            >/tmp/emulator.log 2>&1 &
        emulator_pid=$!

        window=""
        for _ in $(seq 1 60); do
            window=$(xdotool search --onlyvisible --name "mGBA" 2>/dev/null | head -n 1 || true)
            if [ -n "$window" ]; then break; fi
            sleep 0.1
        done
        if [ -z "$window" ]; then
            echo "mGBA did not create a visible window" >&2
            exit 1
        fi

        eval "$(xdotool getwindowgeometry --shell "$window")"
        if [ "$WIDTH" -ne 480 ] || [ "$HEIGHT" -ne 320 ]; then
            echo "Unexpected mGBA viewport: ${WIDTH}x${HEIGHT}" >&2
            exit 1
        fi

        capture() {
            scrot -a "$X,$Y,$WIDTH,$HEIGHT" "$1"
            test -s "$1"
            printf "%s %s\n" "$1" "$(stat -c %s /tmp/audio.raw 2>/dev/null || echo 0)" \
                >>build/mgba-test/audio-events.log
        }

        gray_at() {
            convert "$1" -colorspace Gray \
                -format "%[fx:int(255*u.p{$2,$3})]" info:
        }

        assert_white() {
            value=$(gray_at "$1" "$2" "$3")
            if [ "$value" -lt 240 ]; then
                echo "Expected white pixel at $2,$3 in $1; got $value" >&2
                exit 1
            fi
        }

        assert_black() {
            value=$(gray_at "$1" "$2" "$3")
            if [ "$value" -gt 15 ]; then
                echo "Expected black pixel at $2,$3 in $1; got $value" >&2
                exit 1
            fi
        }

        tap_key() {
            xdotool windowfocus --sync "$window"
            xdotool keydown --window "$window" "$1"
            # Stay below the libgba 18-frame repeat delay while spanning enough
            # stressed frames that a C58 mixer/UI pass cannot miss the tap.
            sleep 0.22
            xdotool keyup --window "$window" "$1"
            sleep 0.20
        }

        assert_changed() {
            changed=$(compare -metric AE "$1" "$2" null: 2>&1 || true)
            if [ "${changed:-0}" -lt "$3" ]; then
                echo "Expected $1 and $2 to differ by at least $3 pixels; got ${changed:-0}" >&2
                exit 1
            fi
        }

        # Avoid screenshot polling here: repeated X11 captures can starve the
        # cycle-accurate emulator during startup.
        sleep 2
        capture build/mgba-test/boot.png
        assert_black build/mgba-test/boot.png 20 20
        assert_white build/mgba-test/boot.png 240 10
        assert_white build/mgba-test/boot.png 208 288
        assert_black build/mgba-test/boot.png 20 316

        # R+D-pad edits Pitch in Performance view and updates its readout.
        xdotool keydown --window "$window" s
        tap_key Up
        xdotool keyup --window "$window" s
        sleep 0.12
        capture build/mgba-test/pitch-up.png
        assert_changed build/mgba-test/boot.png \
            build/mgba-test/pitch-up.png 10
        tap_key Return
        sleep 0.5
        capture build/mgba-test/pitch-up-edit.png
        tap_key Return
        sleep 0.5
        xdotool keydown --window "$window" s
        tap_key Down
        xdotool keyup --window "$window" s
        sleep 0.12

        # A+D-pad edits Range without moving the position line.
        assert_black build/mgba-test/boot.png 320 12
        xdotool keydown --window "$window" x
        tap_key Up
        xdotool keyup --window "$window" x
        sleep 0.12
        capture build/mgba-test/range-up.png
        assert_white build/mgba-test/range-up.png 320 12
        assert_white build/mgba-test/range-up.png 240 10
        xdotool keydown --window "$window" x
        if [ "$TEST_ROM" = ambient-granulator-for-gba.gba ] \
                || [ "$TEST_ROM" = ambient-granulator-for-gba-max-load.gba ]; then
            tap_key Down
        else
            tap_key Right
        fi
        xdotool keyup --window "$window" x
        sleep 0.12

        tap_key Right
        capture build/mgba-test/position-right.png
        assert_black build/mgba-test/position-right.png 240 10
        assert_white build/mgba-test/position-right.png 242 10

        tap_key Return
        sleep 0.5
        capture build/mgba-test/edit.png
        assert_white build/mgba-test/edit.png 2 2
        assert_black build/mgba-test/edit.png 152 40

        tap_key Right
        capture build/mgba-test/edit-right.png
        assert_white build/mgba-test/edit-right.png 152 40
        assert_black build/mgba-test/edit-right.png 392 40
        assert_changed build/mgba-test/edit.png build/mgba-test/edit-right.png 200

        xdotool keydown --window "$window" z
        tap_key Left
        xdotool keyup --window "$window" z
        sleep 0.12
        capture build/mgba-test/edit-value.png
        assert_changed build/mgba-test/edit-right.png build/mgba-test/edit-value.png 10
        xdotool keydown --window "$window" z
        tap_key Right
        xdotool keyup --window "$window" z
        sleep 0.12

        tap_key Return
        sleep 0.5
        capture build/mgba-test/performance.png
        assert_black build/mgba-test/performance.png 20 20

        xdotool keydown --window "$window" x
        sleep 1
        capture build/mgba-test/grains.png
        xdotool keyup --window "$window" x
        sleep 0.15
        assert_changed build/mgba-test/performance.png build/mgba-test/grains.png 20

        tap_key a
        capture build/mgba-test/freeze.png
        assert_changed build/mgba-test/grains.png build/mgba-test/freeze.png 10
        tap_key a
        capture build/mgba-test/unfreeze.png
        assert_changed build/mgba-test/freeze.png build/mgba-test/unfreeze.png 10

        tap_key BackSpace
        sleep 0.5
        capture build/mgba-test/browser.png
        assert_white build/mgba-test/browser.png 2 2
        assert_black build/mgba-test/browser.png 10 40

        tap_key Down
        capture build/mgba-test/browser-next.png
        assert_changed build/mgba-test/browser.png \
            build/mgba-test/browser-next.png 20

        tap_key x
        sleep 0.5
        capture build/mgba-test/browser-load.png
        assert_black build/mgba-test/browser-load.png 20 20

        # Keep producing grains long enough for an audio regression capture.
        xdotool keydown --window "$window" x
        sleep "$SOAK_SECONDS"
        xdotool keyup --window "$window" x
        sleep 0.5
        capture build/mgba-test/later.png

        # The status row must still report U000 after the interaction sequence.
        zero_rows=$(convert build/mgba-test/later.png \
            -crop 34x14+388+288 +repage -filter point -resize 17x7! \
            -depth 8 gray:- | perl -e '\''
                local $/;
                my @pixels = unpack("C*", <STDIN>);
                my @expected = (14, 17, 19, 21, 25, 17, 14);
                for my $digit (0 .. 2) {
                    my @rows;
                    for my $y (0 .. 6) {
                        my $row = 0;
                        for my $x (0 .. 4) {
                            my $pixel = $pixels[$y * 17 + $digit * 6 + $x];
                            $row |= 1 << (4 - $x) if $pixel > 128;
                        }
                        push @rows, $row;
                    }
                    exit 1 if join(",", @rows) ne join(",", @expected);
                }
                print "ok";
            '\'' || true)
        if [ "$zero_rows" != ok ]; then
            echo "Audio underrun status is not U000" >&2
            exit 1
        fi

        mixer_load=$(convert build/mgba-test/later.png \
            -crop 22x14+448+288 +repage -filter point -resize 11x7! \
            -depth 8 gray:- | perl -e '\''
                local $/;
                my @pixels = unpack("C*", <STDIN>);
                my @font = (
                    "14,17,19,21,25,17,14", "4,12,4,4,4,4,14",
                    "14,17,1,2,4,8,31", "30,1,1,14,1,1,30",
                    "2,6,10,18,31,2,2", "31,16,16,30,1,1,30",
                    "14,16,16,30,17,17,14", "31,1,2,4,8,8,8",
                    "14,17,17,14,17,17,14", "14,17,17,15,1,1,14"
                );
                my $value = 0;
                for my $column (0 .. 1) {
                    my @rows;
                    for my $y (0 .. 6) {
                        my $row = 0;
                        for my $x (0 .. 4) {
                            my $pixel = $pixels[$y * 11 + $column * 6 + $x];
                            $row |= 1 << (4 - $x) if $pixel > 128;
                        }
                        push @rows, $row;
                    }
                    my $pattern = join(",", @rows);
                    my ($digit) = grep { $font[$_] eq $pattern } 0 .. 9;
                    exit 1 if !defined $digit;
                    $value = $value * 10 + $digit;
                }
                print $value;
            '\'' || true)
        if [ -z "$mixer_load" ] || [ "$mixer_load" -gt 60 ]; then
            echo "Mixer load exceeds the 60% deadline target: C${mixer_load:-??}" >&2
            exit 1
        fi

        kill "$emulator_pid" 2>/dev/null || true
        wait "$emulator_pid" 2>/dev/null || true
        emulator_pid=""

        python3 - <<'\''PY'\''
import math
import struct
from pathlib import Path

raw = Path("/tmp/audio.raw").read_bytes()
events = Path("build/mgba-test/audio-events.log").read_text().splitlines()
steady_start = next(
    int(line.rsplit(" ", 1)[1])
    for line in events
    if line.startswith("build/mgba-test/browser-load.png ")
)
raw = raw[steady_start:]
if len(raw) < 16384 or len(raw) % 4:
    raise SystemExit("mGBA stereo capture is missing or malformed")

samples = struct.unpack(f"<{len(raw) // 2}h", raw)
left = samples[0::2]
right = samples[1::2]
active = [
    (l, r) for l, r in zip(left, right)
    if abs(l) > 100 or abs(r) > 100
]
if len(active) < 10000:
    raise SystemExit(f"audio capture contains too little active material: {len(active)} frames")
left_rms = math.sqrt(sum(l * l for l, _ in active) / len(active))
right_rms = math.sqrt(sum(r * r for _, r in active) / len(active))
different = sum(l != r for l, r in active)
maximum_jump_left = max(abs(b - a) for a, b in zip(left, left[1:]))
maximum_jump_right = max(abs(b - a) for a, b in zip(right, right[1:]))
if left_rms < 300 or right_rms < 300:
    raise SystemExit(f"silent stereo capture: RMS L={left_rms:.1f} R={right_rms:.1f}")
if different < len(active) // 10:
    raise SystemExit("left and right audio paths are unexpectedly identical")
# Analyse the steady grain run after the browser interaction. Parameter-change
# transitions have dedicated host regressions, while the embedded source PCM
# can itself contain legitimate adjacent changes larger than the old limit.
if maximum_jump_left > 8192 or maximum_jump_right > 8192:
    raise SystemExit(
        "click-like audio discontinuity: "
        f"max jump L={maximum_jump_left} R={maximum_jump_right}"
    )
print(
    "Instrument audio passed: "
    f"{len(active)} active frames, RMS L={left_rms:.1f} R={right_rms:.1f}, "
    f"max jump L={maximum_jump_left} R={maximum_jump_right}"
)
PY
    '

printf 'mGBA instrument regression passed. Screenshots: %s\n' "$artifact_dir"
