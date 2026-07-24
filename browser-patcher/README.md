# Offline sample patcher

Open `dist/ambient-granulator-gba-patcher.html` directly in a current browser.
No server, upload, account, or internet connection is required. Load a
compatible Ambient Granulator `.gba`, add browser-decodable audio, edit the
sample pool, and export a new patched ROM.

The waveform's white section and preview playback use the same resampled,
gain-applied signed PCM8 data written to the ROM. Grey waveform regions are
outside the trim boundaries.

To rebuild and test the single-file patcher:

```sh
npm run build
npm test
```
