# Changelog

## 0.12 — 2026-07-31

- Add an idle-only kiosk mode that changes to a different random sample every
  30–60 seconds, chooses a pitch from −12, −7, 0, +7, or +12 semitones,
  plays the configured grain burst with Freeze off, and then restores Freeze.
- Permanently stop kiosk automation on the first recognised button input and
  show `KIOSK MODE` on the default Performance waveform view while it is active.

## 0.11 — 2026-07-31

- Replace the original ten-sample bank with five author-created piano
  recordings released under CC0 1.0.
- Choose a pseudorandom embedded sample on every boot and advance the startup
  state in SRAM for new choices across sessions.
- Begin an eight-grain burst immediately, wait for the complete burst to seed
  the reverb, and enable Freeze automatically for an instant sustained
  texture.
- Cancel the pending automatic Freeze when the performer presses a control,
  preserving immediate manual control during startup.
- Seed grain-position randomization from the per-boot startup state.
- Add a dedicated startup selection and Freeze-delay regression.
- Extend the mGBA regression to require unattended startup to reach `FZ` and
  make its browser assertions independent of the randomly selected sample.
- Isolate the FIFO continuity profile from the sample bank and UI so its
  triangle-wave capture measures audio handoffs under screen DMA directly.
- Make sample-bank tests independent of particular sample filenames, formats,
  and durations.

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
