#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import struct
import sys
import tempfile
import wave
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "build_sample_bank", ROOT / "tools" / "build_sample_bank.py"
)
assert SPEC and SPEC.loader
bank_tool = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = bank_tool
SPEC.loader.exec_module(bank_tool)


def make_silent_wav(path: Path) -> None:
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(16384)
        output.writeframes(b"\x00\x00" * 512)


def main() -> None:
    prepared_samples = [
        bank_tool.prepare_sample(path) for path in sorted((ROOT / "samples").glob("*.wav"))
    ]
    assert prepared_samples
    for prepared in prepared_samples:
        if any(prepared.pcm):
            assert prepared.peak == 124
        else:
            assert prepared.peak == 0

    piano = bank_tool.prepare_sample(ROOT / "samples" / "piano.wav")
    assert piano.source_channels == 2
    assert piano.source_bits == 24
    assert piano.source_rate == 48000
    assert len(piano.pcm) == 16 * 16384
    assert piano.peak == 124
    assert len(piano.minimums) == 240 and len(piano.maximums) == 240

    with tempfile.TemporaryDirectory() as directory:
        silent_path = Path(directory) / "silent.wav"
        make_silent_wav(silent_path)
        silent = bank_tool.prepare_sample(silent_path)
        assert silent.peak == 0
        assert set(silent.pcm) == {0}

    bank = bank_tool.build_bank([piano], 512 * 1024)
    assert bank[:8] == b"GBAGRN01"
    assert struct.unpack_from("<I", bank, 16)[0] == 1
    assert struct.unpack_from("<I", bank, 24)[0] == 16384
    pcm_offset, pcm_length, crc, waveform_offset, waveform_length = struct.unpack_from(
        "<IIIII", bank, 64 + 32
    )
    assert pcm_offset % 32 == 0 and waveform_offset % 32 == 0
    assert pcm_length == len(piano.pcm)
    assert waveform_length == 480
    assert zlib.crc32(bank[pcm_offset : pcm_offset + pcm_length]) & 0xFFFFFFFF == crc
    assert bank[waveform_offset : waveform_offset + 240] == piano.minimums
    assert bank[waveform_offset + 240 : waveform_offset + 480] == piano.maximums

    print(
        f"all {len(prepared_samples)} samples normalized; silence, alignment, "
        "waveform and CRC passed"
    )


if __name__ == "__main__":
    main()
