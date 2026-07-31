# Ambient Granulator for GBA v0.13

This release retunes unattended kiosk textures for a clearer, more varied
result. The normal preset now starts at `REV 100`, producing a completely wet
texture while retaining the established stereo FDN width model.

Every kiosk refresh chooses a different random sample, a centre anywhere
across the waveform, and a symmetric Range from 0–128 pixels. Transposition is
now restricted to 0, +7, or +12 semitones; the muddy −7 and −12 choices have
been removed. The cycle still disables Freeze, plays the configured grain
burst, waits for those grains to seed the reverb, and then restores Freeze.

Kiosk mode remains strictly unattended. The first recognised GBA button input
permanently stops its automation for that session and cancels any pending
automatic Freeze. Emulator or host gestures mapped to GBA buttons behave the
same way.

Tests cover the full position and Range bounds, narrow and wide selections,
the three nonnegative pitch choices, non-repeating sample selection, randomized
30–60 second timing, and permanent first-input cancellation. The project
author confirmed that the current tuning sounds improved in mGBA.

The five embedded piano recordings were created by Sebastian Tomczak and
released under CC0 1.0. The release includes the sample licence and provenance
notice.

Verification: host DSP and sample-bank tests, offline patcher tests, and mGBA
UI/audio/FIFO/max-load regressions passed. A basic physical GBA-family
boot/play test was previously reported with an unidentified budget flashcart.
The v0.13 tuning remains emulator-verified; full flashcart and analogue-audio
qualification has not been recorded.
