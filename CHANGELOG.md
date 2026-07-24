# Changelog

## 0.1 — 2026-07-14

- Rebuild Ambient Granulator natively for Game Boy Advance.
- Add the monochrome Performance, Edit, and ROM sample-browser views.
- Add Performance-view A+D-pad Range and R+D-pad Pitch shortcuts.
- Add Fine and F Dev cent controls beneath Pitch and P Dev, including Q16
  per-grain cent tuning and a live fine-pitch readout.
- Add a Performance-view pitch readout and an eight-scanline bottom safe area
  for original GBA LCD visibility.
- Add four PCM8 grain voices, Q16 pitch, envelopes, gain, pan/deviation,
  sample-accurate Free/Sync timing, jitter, complete bursts, and live markers.
- Add a four-line stereo FDN with wet/dry control, size, damping, stable Freeze,
  post-output HPF/LPF, and adjustable feedback through 99.9%.
- Add the deterministic 8 MiB `GBAGRN01` bank and embed all ten supplied WAV
  files as normalised 16,384 Hz signed PCM8.
- Add a single-file offline browser patcher with exact PCM8 preview, trim,
  baked gain, rename, reorder, removal, capacity accounting, CRC regeneration,
  GBA header preservation, and patched-ROM export.
- Add a mirrored FIFO handoff that resumes on-time blocks at sample zero and
  advances only for complete requests made during actual IRQ lateness,
  removing periodic 16-sample drops at audio-buffer boundaries.
- Add waveform-continuing voice-steal/sample-change tails, move all four FDN
  delay lines into fast RAM, batch grain rendering, cache voice state, and use
  pre-scaled stereo envelope ramps.
- Make voice-steal tails reach an exact zero endpoint and enable a gentle
  4 kHz LPF in the normal preset to suppress sharp PCM8 source transients.
- Retain the complete four-line FDN plus LPF in the official maximum-load
  profile; its corrected active-path 60-second mGBA soak holds U000 at C58.
- Add single-grain, maximum-gain, maximum-feedback, impulse-reverb,
  Freeze-after-impulse, repeated-Freeze, HPF/LPF response, and real-sample
  maximum-load listening renders.
- Verify that post-output LPF changes a frozen texture while every FDN delay
  sample remains unchanged, and directly check normalization of all ten WAVs.
- Add pinned devkitARM builds, independent ROM checks, host DSP/audio renders,
  patcher tests, and automated mGBA UI/stereo/click/underrun regression,
  including a deterministic triangle-wave FIFO continuity capture.
- Package checksum-pinned maximum-load and patcher-produced hardware-test ROMs
  with a structured physical-GBA acceptance and sign-off record.
