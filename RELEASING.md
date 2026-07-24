# Releasing Ambient Granulator for GBA

This is the project’s GitHub release etiquette. Its main rule is simple:
**the tag must identify the exact commit from which every uploaded asset was
built.** ROMs and patchers are release assets, not repository files.

## What a release means

Use the project’s hundredth-step release tags: start at `v0.1`, then use
`v0.11`, `v0.12`, and so on. A release has:

- one clean, committed source revision on `main`;
- an annotated tag on that revision;
- reproducible assets made by `scripts/release.sh vX.Y.Z`;
- a GitHub Release containing the user-facing artifacts and their checksums.

Do not move a published tag. If a fix is needed, publish the next hundredth
step instead.

## Publication gate

Before making a public release, make sure that every embedded sample may be
redistributed. The current [sample licence](samples/LICENSE.md) records that
`piano.wav` is CC BY 4.0 and the remaining bundled WAVs are copyright-free
source material approved for redistribution. Include `SAMPLE_LICENSE.md` with
every release, retain the piano attribution, and document the provenance of
any newly added sample before shipping it.

An informal physical test has been reported: the ROM generally boots and
works on a GBA-family console using an unidentified inexpensive flashcart.
That is useful real-world evidence, but it is not a hardware qualification;
do not name the cart, imply broad flashcart compatibility, or claim an
analogue stereo test. The release may truthfully say **“emulator-verified;
basic physical-hardware boot/play test reported”**. Use
`HARDWARE_ACCEPTANCE.md` only when the console, cart, result, and longer checks
can be recorded.

## Release steps

1. Land all changes on `main`, then start from a clean working tree. For the
   first public release, create the GitHub repository and push `main` before
   continuing.
2. Choose the next version (for example, `v0.11` after `v0.1`) and update it
   consistently:
   - add a dated, user-facing entry to `CHANGELOG.md`;
   - update the title and status in `RELEASE_NOTES.md` and `README.md`;
   - update `HARDWARE_ACCEPTANCE.md` filenames if the hardware record ships
     with this release;
   - update the default in `scripts/release.sh` only when it should point to
     the new current release.
3. Commit the release metadata, push `main`, and verify that `git status` is
   clean. Do not build or tag from an uncommitted tree.
4. Build and verify the release from that commit:

   ```sh
   scripts/release.sh v0.1
   ```

   This runs the host, patcher, mGBA UI/audio, FIFO-continuity, and 60-second
   maximum-load checks, then writes the distributable files to
   `build/release/`.
5. Check the emitted checksums and inspect the four user-facing assets:

   ```sh
   cd build/release
   shasum -a 256 -c SHA256SUMS.txt
   ```

6. Tag the build commit and push the tag only after the checks pass:

   ```sh
   git tag -a v0.1 -m "Ambient Granulator for GBA v0.1"
   git push origin v0.1
   ```

7. Create the GitHub Release from that tag. Upload these files directly from
   `build/release/`:
   - `ambient-granulator-for-gba-v0.1.gba`
   - `ambient-granulator-for-gba-max-load-v0.1.gba`
   - `ambient-granulator-for-gba-patched-test-v0.1.gba`
   - `ambient-granulator-gba-patcher-v0.1.html`
   - `SHA256SUMS.txt`
   - `SAMPLE_LICENSE.md`

   GitHub supplies source archives for the tag automatically; do not attach
   generated object files, emulator captures, or the `build/` directory.
8. Keep release notes short: lead with the musical/user-facing change, list
   important compatibility or behaviour changes, state the verification level,
   link to `HARDWARE_ACCEPTANCE.md`, and mention sample licensing where needed.
9. Confirm the GitHub release tag points to the already-built commit and that
   all asset names and checksum entries match. Leave `main` clean afterward.

## Suggested release-note footer

> Verification: host DSP and sample-bank tests, offline patcher tests, and
> mGBA UI/audio/FIFO/max-load regressions passed. A basic physical GBA-family
> boot/play test was reported with an unidentified budget flashcart. Full
> flashcart and analogue-audio qualification has not been recorded.

## Why this is the house style

This follows ALYNXDJ’s useful practice: commit the release metadata first,
build from the clean committed revision, and tag that precise revision before
creating the GitHub Release. It makes the binary, checksum, source, and tag
auditable as one release rather than four loosely related files.
