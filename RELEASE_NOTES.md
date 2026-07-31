# Ambient Granulator for GBA v0.12

This release adds an unattended kiosk mode for continuously evolving ambient
textures. The ROM opens on the Performance waveform with `KIOSK MODE` visible.
While no control input has been recognised, it waits a newly randomized
30–60 seconds between cycles, loads a different random sample, disables
Freeze, chooses −12, −7, 0, +7, or +12 semitones, and plays the configured
grain burst. Once those grains have seeded the stereo reverb, Freeze is
restored and the texture sustains until the next cycle.

The first recognised GBA button input permanently stops kiosk automation for
that session and immediately cancels any pending automatic Freeze. Emulator or
host gestures mapped to GBA buttons behave the same way. The performer keeps
direct control without the kiosk resuming later.

The existing randomized power-on behavior remains: one of the five embedded
piano recordings begins an eight-grain burst immediately, seeds the reverb,
and reaches Freeze automatically if no input interrupts it. The pseudorandom
startup state continues to advance in the first 12 bytes of a conventional
32 KiB `SRAM_V113` save.

Tests cover the 30–60 second bounds, non-repeating sample choice, five allowed
pitch values, permanent first-input cancellation, the default waveform view,
the visible kiosk indicator, and its dismissal on input. The project author
also confirmed kiosk mode working in mGBA.

The five embedded piano recordings were created by Sebastian Tomczak and
released under CC0 1.0. The release includes the sample licence and provenance
notice.

Verification: host DSP and sample-bank tests, offline patcher tests, and mGBA
UI/audio/FIFO/max-load regressions passed. A basic physical GBA-family
boot/play test was previously reported with an unidentified budget flashcart.
Kiosk mode remains emulator-verified; full flashcart and analogue-audio
qualification has not been recorded.
