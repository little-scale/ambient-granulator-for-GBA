# Ambient Granulator for GBA v0.1

This first GBA release delivers the complete four-voice instrument, stereo FDN
reverb and Freeze, post-reverb filters, embedded sample browser, and offline
single-file ROM patcher described in `task.txt`.

Feedback is adjustable from 0.0% through 99.9% in 0.1% steps. Freeze remains a
separate exact-unity mode that stops new FDN input without silencing new dry
grains. The Direct Sound streamer now mirrors the following block and resumes
an on-time handoff at sample zero, correcting an earlier 16-sample offset that
could drop the start of each new block and create audible discontinuities.
Actual late handoffs advance only by complete FIFO requests. A deterministic
triangle-wave emulator capture now checks that handoffs neither drop nor
repeat samples.
A waveform-continuing 32-sample tail now smooths busy-voice reuse and sample
changes and reaches an exact zero endpoint. The normal preset now starts with
a gentle 4 kHz LPF to suppress sharp PCM8 source edges while retaining full
user control through LPF bypass. All four FDN delay lines live in fast internal RAM, while block-based
grain mixing, cached voice state, and pre-scaled stereo envelope ramps reduce
the ARM7 hot path. The official maximum-load profile retains the complete
four-line FDN and a 2 kHz LPF, bypasses HPF, and holds U000 at C58 during the
corrected 60-second active-path mGBA soak.

The stock ROM now contains all ten WAV files supplied in `samples/`. Every
non-silent input is downmixed, resampled to 16,384 Hz, normalised to −0.3 dBFS,
and quantised to signed PCM8 during the deterministic build.

Automated verification covers host DSP/unit tests; dedicated single-grain,
maximum-gain, maximum-feedback, impulse-reverb, Freeze-after-impulse,
repeated-Freeze, HPF/LPF-response, and real-sample maximum-load WAV renders;
LPF shaping of frozen output without changing the FDN memories; normalization
of all ten samples; GBA header and sample-bank checks; browser-patched ROM
reopen/CRC validation; and mGBA interaction, stereo, steady-grain
discontinuity, deterministic FIFO continuity, load, and zero-underrun
regressions.

The release is emulator-verified. Physical flashcart boot, ten-minute load,
60-second hardware Freeze, and analogue stereo checks remain the documented
manual release gates. The release bundle includes checksum-pinned normal,
maximum-load, and patcher-produced test ROMs plus a structured
`HARDWARE_ACCEPTANCE.md` result sheet so those gates can be run without a
development toolchain.
