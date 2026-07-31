# Ambient Granulator for GBA

**Current release: v0.11 — emulator-verified; basic physical-hardware
boot/play test reported.**

Ambient Granulator for GBA is a native Game Boy Advance granular instrument.
It combines a four-voice grain engine, sample-accurate burst scheduler, stereo
feedback-delay-network reverb, stable Freeze, post-reverb filters, an embedded
ROM sample browser, and a completely offline sample patcher.

The included build embeds all WAV files currently in `samples/` as signed
mono PCM8 at exactly 16,384 Hz. Each non-silent file is peak-normalised to
−0.3 dBFS before quantisation. The ROM uses a black-and-white 240×160 interface
inspired by the original Ambient Granulator for Nintendo DS while rebuilding
the hardware layer for GBA Direct Sound.

## Quick start

Copy `ambient-granulator-for-gba.gba` to a GBA flashcart or open it in mGBA.
For stereo panning and reverb, use headphones or a stereo line capture; the
GBA's built-in speaker is mono.

At startup, the instrument chooses a sample from the embedded bank, begins an
eight-grain burst immediately, lets the grains seed the reverb, and then turns
Freeze on automatically. The pseudorandom startup state is advanced in SRAM,
so subsequent boots produce new sample and grain choices. Pressing a control
before the automatic Freeze step cancels that step and hands control to you.

The release bundle also includes checksum-pinned maximum-load and
patcher-produced test ROMs. Use `HARDWARE_ACCEPTANCE.md` to run and record the
complete physical-GBA qualification procedure.

### Performance view

| Control | Action |
| --- | --- |
| D-pad | Move the grain position by 1 pixel horizontally or 8 vertically |
| Hold A | Continuously produce complete grain bursts |
| A + Left/Right | Decrease/increase Range by 1 pixel while triggering |
| A + Down/Up | Decrease/increase Range by 16 pixels while triggering |
| R + Left/Right | Decrease/increase Pitch by 1 semitone |
| R + Down/Up | Decrease/increase Pitch by 12 semitones |
| Tap B | Produce one configured burst |
| L | Toggle Freeze |
| Start | Open/close Edit view |
| Select | Open the ROM sample browser |

### Edit view

Use the D-pad to select a parameter. Hold B and press Left/Right for fine
adjustment or Up/Down for coarse adjustment. Releasing B without editing
triggers a burst.

Pitch has semitone Pitch/P Dev controls plus Fine (±100 cents) and F Dev
(0–100 cents), with 1-cent fine and 10-cent coarse edits. Feedback ranges from
**0.0% to 99.9%**. Fine adjustment changes 0.1%; coarse
adjustment changes 1%. Freeze uses exact unity feedback internally, blocks new
reverb input, and leaves the stored Feedback setting unchanged. New dry grains
remain audible during Freeze whenever REV is below 100%.

The normal preset starts with a gentle 4 kHz LPF to suppress sharp PCM8 source
edges. LPF remains adjustable from 200 Hz through bypass (`OFF`), and HPF is
independently adjustable.

In the sample browser, use Up/Down to move one row, Left/Right to page, A or B
to load, and Select to cancel.

The status line shows semitone and cent pitch as `P+00 F+000` and ends with
`U000 Cxx`:
`U000` is the audio-underrun count and `Cxx` is the worst measured mixer load
as a percentage of its block deadline. The line sits one UI row above the
physical bottom edge; the lowest eight scanlines are deliberately unused for
visibility on original GBA displays.

## Offline sample patcher

Open
`browser-patcher/dist/ambient-granulator-gba-patcher.html` directly in a
current browser. It is one self-contained file and does not need a server,
internet connection, upload, or account.

1. Load a compatible Ambient Granulator `.gba`.
2. Add one or more browser-decodable audio files.
3. Rename, reorder, remove, trim, adjust baked gain, and preview samples.
4. Export the patched ROM.

Imports are downmixed equally, normalised once to −0.3 dBFS, resampled to
16,384 Hz, and encoded as signed PCM8. The white waveform and preview use the
same quantised data written on export. The patcher validates the GBA header,
requires exactly one `GBAGRN01` bank, preserves the complete 0xC0-byte header,
rebuilds waveform summaries and CRC32 values, and reopens its result before
offering the download.

The fixed 8 MiB bank supports up to 64 samples and roughly 8.5 minutes of mono
PCM8 audio in total.

## Reproducible build

Docker Desktop or another Docker-compatible engine is the only required host
tool. The build is pinned to the official multi-architecture
`devkitpro/devkitarm:20260610` image.

```sh
scripts/setup.sh
scripts/build.sh
```

The build deterministically converts sorted WAV files from `samples/`, embeds
an 8 MiB bank, invokes `gbafix`, pads to a 16 MiB power-of-two cartridge image,
independently validates the GBA header, and prints the SHA-256 digest. The GBA
header title is `AMBGRANULAR` and game code is `AGRN`.

To rebuild the standalone patcher:

```sh
npm --prefix browser-patcher run build
```

## Verification

```sh
scripts/test-host.sh
npm --prefix browser-patcher test
scripts/test-mgba.sh
scripts/test-fifo-mgba.sh
scripts/test-max-load-mgba.sh 60
```

Host tests cover deterministic sample conversion, bank bounds and CRCs,
timing, position ranges, pan, wet/dry, pitch, envelope endpoints, filters,
four-voice scheduling, maximum gain saturation, 60-second 99.9% feedback
stability, 60-second Freeze, and LPF shaping of frozen output without any
change to the four FDN delay memories. They also render listening WAVs to
`build/host-tests/` for a single grain, default grains, maximum gain, maximum
feedback, hard-left/right and impulse reverb, Freeze with new dry grains,
Freeze after an impulse, repeated Freeze toggles, HPF/LPF responses, and
maximum load with both smooth and transient-rich source material.

The mGBA regression checks boot pixels, navigation, parameter editing,
Performance/Edit switching, held triggering, grain markers, Freeze on/off,
sample browsing, stereo output, zero underruns, and the absence of click-like
steady-grain discontinuities. The patcher suite creates, reopens,
independently validates, and emulator-boots a two-sample patched ROM.

The audio path uses 512-sample double buffers. Each buffer carries a full
mirror of the following block; the timer handler resumes at sample zero for an
on-time handoff and advances only for complete 16-sample FIFO requests made
during actual interrupt lateness. This prevents screen DMA from reading past a
buffer without dropping the first 16 samples of the next one—the source of the
earlier periodic clicks—without changing the instrument's pitch or filtering
its output. A deterministic triangle-wave mGBA regression now verifies that
buffer handoffs neither drop nor repeat samples. A waveform-continuing
32-sample tail separately de-clicks round-robin voice stealing and sample
changes. All four reverb delay lines, the FDN, and the block-based grain mixer
run from fast internal RAM. Pre-scaled stereo envelope ramps avoid per-sample
envelope multiplications while preserving smooth attack/release motion.

The official maximum-load profile keeps four grains, the complete four-line
FDN, 99% feedback, animation, and a 2 kHz LPF. HPF is bypassed in that stress
profile to preserve timing margin; both HPF and LPF remain available in the
normal instrument. The corrected 60-second active-path mGBA soak holds `U000`
at `C58`, below the `C60` deadline ceiling.

## Physical-hardware acceptance

Emulator and host tests cannot establish flashcart boot or analogue output.
Before publishing a hardware-qualified release, test on at least one
GBA-family system and record the console, flashcart/firmware, ROM SHA-256,
duration, final underrun count, and stereo result.

- Run maximum-load playback for ten minutes with `U000` throughout.
- Hold Freeze for 60 seconds and toggle it repeatedly.
- Test the stock and browser-patched ROMs and every embedded sample.
- Edit parameters while audio runs and listen for clicks.
- Confirm waveform integrity and immediate button response.
- Verify stereo panning and reverb with headphones or a stereo capture.

Until that checklist has been completed on physical hardware, the build should
be treated as emulator-verified rather than hardware-qualified.
There is one informal report of normal boot and playback on a GBA-family
console using an unidentified inexpensive flashcart; it supports a basic
real-hardware claim but does not replace the recorded acceptance checks.
The release bundle's `HARDWARE_ACCEPTANCE.md` provides the exact ROMs, timed
steps, per-sample checks, and sign-off fields for this gate.

## Releasing

See [`RELEASING.md`](RELEASING.md) for the GitHub release procedure: clean
committed build, annotated tag, checksums, release assets, licence gate, and
the wording appropriate for the current level of hardware evidence.

## Licence and provenance

Software is MIT licensed; see `LICENSE`. Sebastian Tomczak created the five
bundled piano recordings and released them under CC0 1.0; see
`samples/LICENSE.md` for the public-domain dedication and provenance. The NDS
reference is MIT licensed and is used as the behavioural and visual reference.

- [devkitPro](https://devkitpro.org/)
- [GBATEK](https://mgba-emu.github.io/gbatek/)
- [mGBA](https://mgba.io/)
- [Ambient Granulator for NDS](https://github.com/little-scale/ambient-granulator-for-NDS)
