# Physical GBA Acceptance Record

Use this record to qualify Ambient Granulator for GBA v0.1 on real hardware.
Do not mark the release hardware-qualified if any required check fails.

## Preliminary hardware evidence

The project author reports that the ROM generally boots and works on a
GBA-family console using an unidentified inexpensive flashcart. The console,
cart model, firmware, date, and detailed results were not recorded, so this is
not a completed acceptance record or a compatibility claim for that cart.

## Test setup

| Field | Result |
| --- | --- |
| Date and tester |  |
| GBA-family model |  |
| Flashcart and firmware |  |
| Headphones or stereo capture device |  |
| Battery/power source |  |
| Release SHA-256 file verified | PASS / FAIL |

From the release directory, verify every supplied artifact before copying ROMs
to the flashcart:

```sh
shasum -a 256 -c SHA256SUMS.txt
```

The release contains three ROMs for distinct checks:

- `ambient-granulator-for-gba-v0.1.gba`: normal instrument.
- `ambient-granulator-for-gba-max-load-v0.1.gba`: four active grain voices,
  500 ms grains, maximum trigger density, 99% feedback, the complete four-line
  FDN, animated markers, and a 2 kHz LPF. HPF is deliberately bypassed in this
  stress profile.
- `ambient-granulator-for-gba-patched-test-v0.1.gba`: patcher-produced ROM
  with the ten release samples plus an appended `PATCHER TEST TONE`.

## Normal instrument

Boot `ambient-granulator-for-gba-v0.1.gba` and use headphones or a stereo
line capture.

| Required check | Result | Notes |
| --- | --- | --- |
| Boots to the monochrome Performance view | PASS / FAIL |  |
| Waveform is intact and the lowest display row is unused | PASS / FAIL |  |
| D-pad movement and A/B/L/R/Start/Select respond immediately | PASS / FAIL |  |
| A+D-pad changes Range; R+D-pad changes Pitch | PASS / FAIL |  |
| Edit parameters take effect within one audio block and do not click | PASS / FAIL |  |
| Hard-left and hard-right Pan are distinct in headphones | PASS / FAIL |  |
| Stereo FDN output is audible and not collapsed to mono | PASS / FAIL |  |
| Final status remains `U000` | PASS / FAIL |  |

Open the sample browser, load every entry, trigger several grains, and confirm
that its waveform and audio are valid.

| Embedded sample | Audio | Waveform | No click on load |
| --- | --- | --- | --- |
| `1` | PASS / FAIL | PASS / FAIL | PASS / FAIL |
| `110bpm F - 01 - Hiskee Vocalpack` | PASS / FAIL | PASS / FAIL | PASS / FAIL |
| `130bpm Am - 05 - Hiskee Vocalpack` | PASS / FAIL | PASS / FAIL | PASS / FAIL |
| `2` | PASS / FAIL | PASS / FAIL | PASS / FAIL |
| `3` | PASS / FAIL | PASS / FAIL | PASS / FAIL |
| `4` | PASS / FAIL | PASS / FAIL | PASS / FAIL |
| `5` | PASS / FAIL | PASS / FAIL | PASS / FAIL |
| `6` | PASS / FAIL | PASS / FAIL | PASS / FAIL |
| `piano` | PASS / FAIL | PASS / FAIL | PASS / FAIL |
| `sample1` | PASS / FAIL | PASS / FAIL | PASS / FAIL |

## Maximum-load and Freeze soak

Boot `ambient-granulator-for-gba-max-load-v0.1.gba`.

1. Hold A continuously for ten minutes without leaving Performance view.
2. Move Position and edit parameters several times while audio continues.
3. Record `Uxxx` and `Cxx` at the end. Acceptance requires `U000`, `C60` or
   lower, responsive input, intact animation, and no audible discontinuities.
4. Build a reverb texture, press L to enable Freeze, release A, and listen for
   at least 60 seconds. The texture must remain stable.
5. Toggle Freeze off/on at least ten times, triggering grains between toggles.

| Stress check | Result | Recorded value or notes |
| --- | --- | --- |
| Ten-minute maximum-load playback | PASS / FAIL | Duration:  |
| No audible clicks or pitch disturbance | PASS / FAIL |  |
| No delayed/missed buttons or corrupted waveform | PASS / FAIL |  |
| Final underrun count | PASS / FAIL | U:  |
| Peak mixer load | PASS / FAIL | C:  |
| 60-second stable Freeze | PASS / FAIL | Duration:  |
| Ten repeated Freeze toggles | PASS / FAIL |  |

## Patched ROM

Boot `ambient-granulator-for-gba-patched-test-v0.1.gba`, open the browser,
select the final `PATCHER TEST TONE`, and trigger it. This is the deterministic
hardware check for the offline patcher's exported bank.

| Required check | Result | Notes |
| --- | --- | --- |
| Patched ROM boots from flashcart | PASS / FAIL |  |
| Eleven samples appear in the browser | PASS / FAIL |  |
| `PATCHER TEST TONE` waveform is intact | PASS / FAIL |  |
| Test tone produces clean audio | PASS / FAIL |  |
| Switching between patched samples does not click | PASS / FAIL |  |
| Final status remains `U000` | PASS / FAIL |  |

## Qualification decision

| Field | Result |
| --- | --- |
| All required checks passed | YES / NO |
| Release classification | HARDWARE-QUALIFIED / EMULATOR-VERIFIED |
| Tester sign-off |  |
| Failure recording or capture links |  |
