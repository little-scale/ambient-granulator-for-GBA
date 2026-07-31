# Ambient Granulator for GBA v0.11

This release turns power-on into a performance gesture. Ambient Granulator now
chooses one of five embedded piano recordings, begins an eight-grain burst
immediately, lets the complete burst populate the stereo reverb, and then
enables Freeze automatically to sustain the resulting texture. Pressing any
control before the automatic Freeze step cancels it and hands control directly
to the performer.

The pseudorandom startup state advances between sessions in the first 12 bytes
of a conventional 32 KiB SRAM save. The ROM declares `SRAM_V113` for emulator
and flashcart save-type detection. Version 0.1 did not store user data, so
there is no save migration requirement.

The embedded bank now contains five original piano recordings created by
Sebastian Tomczak and released under CC0 1.0. The release includes the sample
licence and provenance notice.

Tests now cover startup seed mixing, random sample-index bounds, configurable
startup burst length, the post-grain Freeze delay, and an unattended mGBA boot
that must display `FZ`. Sample-bank checks no longer depend on the previous
filenames or formats.

Verification: host DSP and sample-bank tests, offline patcher tests, and mGBA
UI/audio/FIFO/max-load regressions passed. A basic physical GBA-family
boot/play test was previously reported with an unidentified budget flashcart.
The new startup and SRAM behaviour remains emulator-verified; full flashcart
and analogue-audio qualification has not been recorded.
