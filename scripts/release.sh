#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
version="${1:-v0.1}"
release_dir="$project_dir/build/release"

cd "$project_dir"
scripts/build.sh
scripts/test-host.sh
npm --prefix browser-patcher run build
npm --prefix browser-patcher test
npm --prefix browser-patcher run test:rom
python3 tools/check_rom.py --require-power-of-two \
    build/browser-patcher-test.gba
scripts/test-mgba.sh
scripts/test-mgba.sh build/browser-patcher-test.gba
scripts/test-fifo-mgba.sh
scripts/test-max-load-mgba.sh 60

mkdir -p "$release_dir"
cp ambient-granulator-for-gba.gba \
    "$release_dir/ambient-granulator-for-gba-$version.gba"
cp ambient-granulator-for-gba-max-load.gba \
    "$release_dir/ambient-granulator-for-gba-max-load-$version.gba"
cp build/browser-patcher-test.gba \
    "$release_dir/ambient-granulator-for-gba-patched-test-$version.gba"
cp browser-patcher/dist/ambient-granulator-gba-patcher.html \
    "$release_dir/ambient-granulator-gba-patcher-$version.html"
cp README.md RELEASE_NOTES.md CHANGELOG.md HARDWARE_ACCEPTANCE.md LICENSE \
    "$release_dir/"
cp samples/LICENSE.md "$release_dir/SAMPLE_LICENSE.md"

(
    cd "$release_dir"
    shasum -a 256 \
        "ambient-granulator-for-gba-$version.gba" \
        "ambient-granulator-for-gba-max-load-$version.gba" \
        "ambient-granulator-for-gba-patched-test-$version.gba" \
        "ambient-granulator-gba-patcher-$version.html" \
        > SHA256SUMS.txt
)
cp "$release_dir/SHA256SUMS.txt" "$project_dir/SHA256SUMS.txt"
printf 'Release artifacts: %s\n' "$release_dir"
