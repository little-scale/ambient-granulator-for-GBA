#!/usr/bin/env python3
"""Build a deterministic fixed-capacity GBAGRN01 sample bank."""

from __future__ import annotations

import argparse
import math
import struct
import wave
import zlib
from dataclasses import dataclass
from pathlib import Path


MAGIC = b"GBAGRN01"
BANK_VERSION = 1
SAMPLE_RATE = 16384
HEADER_SIZE = 64
ENTRY_SIZE = 64
MAX_ENTRIES = 64
DATA_OFFSET = HEADER_SIZE + ENTRY_SIZE * MAX_ENTRIES
SAMPLE_FORMAT_PCM8_MONO = 1
WAVEFORM_COLUMNS = 240
WAVEFORM_BYTES = WAVEFORM_COLUMNS * 2
NORMALIZED_PEAK = 10 ** (-0.3 / 20.0)


@dataclass(frozen=True)
class PreparedSample:
    name: str
    pcm: bytes
    minimums: bytes
    maximums: bytes
    source_channels: int
    source_rate: int
    source_bits: int
    peak: int


def _decode_frame(data: bytes, offset: int, width: int) -> int:
    if width == 1:
        return data[offset] - 128
    if width == 2:
        return int.from_bytes(data[offset : offset + 2], "little", signed=True)
    if width == 3:
        raw = int.from_bytes(data[offset : offset + 3], "little", signed=False)
        return raw - (1 << 24) if raw & (1 << 23) else raw
    if width == 4:
        return int.from_bytes(data[offset : offset + 4], "little", signed=True)
    raise ValueError(f"unsupported PCM width: {width * 8} bits")


def read_wav_mono(path: Path) -> tuple[list[float], int, int, int]:
    with wave.open(str(path), "rb") as source:
        if source.getcomptype() != "NONE":
            raise ValueError(f"{path}: compressed WAV is not supported")
        channels = source.getnchannels()
        width = source.getsampwidth()
        rate = source.getframerate()
        frames = source.getnframes()
        if channels < 1 or rate < 1 or frames < 1:
            raise ValueError(f"{path}: empty or invalid WAV")
        raw = source.readframes(frames)

    scale = float(1 << (width * 8 - 1))
    stride = channels * width
    mono: list[float] = []
    append = mono.append
    for frame in range(frames):
        base = frame * stride
        total = 0
        for channel in range(channels):
            total += _decode_frame(raw, base + channel * width, width)
        append(total / (channels * scale))
    return mono, rate, channels, width * 8


def resample_linear(source: list[float], source_rate: int) -> list[float]:
    if source_rate == SAMPLE_RATE:
        return source[:]
    output_count = max(1, (len(source) * SAMPLE_RATE + source_rate // 2) // source_rate)
    output = [0.0] * output_count
    for index in range(output_count):
        position_numerator = index * source_rate
        left = position_numerator // SAMPLE_RATE
        fraction = position_numerator % SAMPLE_RATE
        if left >= len(source) - 1:
            output[index] = source[-1]
        else:
            first = source[left]
            second = source[left + 1]
            output[index] = first + (second - first) * fraction / SAMPLE_RATE
    return output


def normalize_and_quantize(samples: list[float]) -> tuple[bytes, int]:
    source_peak = max(abs(sample) for sample in samples)
    gain = NORMALIZED_PEAK / source_peak if source_peak > 0.0 else 0.0
    quantized = bytearray(len(samples))
    peak = 0
    for index, sample in enumerate(samples):
        value = 0 if gain == 0.0 else int(math.floor(sample * gain * 128.0 + 0.5))
        value = max(-127, min(127, value))
        peak = max(peak, abs(value))
        quantized[index] = value & 0xFF
    return bytes(quantized), peak


def waveform_summary(pcm: bytes) -> tuple[bytes, bytes]:
    signed = [value if value < 128 else value - 256 for value in pcm]
    minimums = bytearray(WAVEFORM_COLUMNS)
    maximums = bytearray(WAVEFORM_COLUMNS)
    count = len(signed)
    for column in range(WAVEFORM_COLUMNS):
        start = column * count // WAVEFORM_COLUMNS
        end = (column + 1) * count // WAVEFORM_COLUMNS
        if end <= start:
            end = min(count, start + 1)
        bucket = signed[start:end] if start < count else signed[-1:]
        minimums[column] = min(bucket) & 0xFF
        maximums[column] = max(bucket) & 0xFF
    return bytes(minimums), bytes(maximums)


def prepare_sample(path: Path) -> PreparedSample:
    mono, rate, channels, bits = read_wav_mono(path)
    resampled = resample_linear(mono, rate)
    pcm, peak = normalize_and_quantize(resampled)
    minimums, maximums = waveform_summary(pcm)
    name = path.stem.encode("ascii", "replace").decode("ascii")[:31] or "SAMPLE"
    return PreparedSample(name, pcm, minimums, maximums, channels, rate, bits, peak)


def align32(value: int) -> int:
    return (value + 31) & ~31


def build_bank(samples: list[PreparedSample], capacity: int) -> bytes:
    if not samples:
        raise ValueError("no WAV samples found")
    if len(samples) > MAX_ENTRIES:
        raise ValueError(f"sample count exceeds {MAX_ENTRIES}")
    if capacity < DATA_OFFSET:
        raise ValueError("bank capacity is smaller than its table")

    bank = bytearray(capacity)
    cursor = DATA_OFFSET
    entries: list[bytes] = []

    for sample in samples:
        waveform_offset = align32(cursor)
        pcm_offset = align32(waveform_offset + WAVEFORM_BYTES)
        end = pcm_offset + len(sample.pcm)
        if end > capacity:
            raise ValueError(
                f"bank capacity exceeded while adding {sample.name}: "
                f"need {end} bytes, have {capacity}"
            )
        bank[waveform_offset : waveform_offset + WAVEFORM_COLUMNS] = sample.minimums
        bank[
            waveform_offset + WAVEFORM_COLUMNS : waveform_offset + WAVEFORM_BYTES
        ] = sample.maximums
        bank[pcm_offset:end] = sample.pcm

        name = sample.name.encode("ascii", "replace")[:31]
        entry = bytearray(ENTRY_SIZE)
        entry[: len(name)] = name
        struct.pack_into(
            "<IIIIII",
            entry,
            32,
            pcm_offset,
            len(sample.pcm),
            zlib.crc32(sample.pcm) & 0xFFFFFFFF,
            waveform_offset,
            WAVEFORM_BYTES,
            0,
        )
        entries.append(bytes(entry))
        cursor = align32(end)

    used_bytes = cursor
    bank[:8] = MAGIC
    struct.pack_into(
        "<IIIIIIIIII",
        bank,
        8,
        BANK_VERSION,
        capacity,
        len(samples),
        used_bytes,
        SAMPLE_RATE,
        ENTRY_SIZE,
        MAX_ENTRIES,
        DATA_OFFSET,
        SAMPLE_FORMAT_PCM8_MONO,
        WAVEFORM_COLUMNS,
    )
    for index, entry in enumerate(entries):
        start = HEADER_SIZE + index * ENTRY_SIZE
        bank[start : start + ENTRY_SIZE] = entry
    return bytes(bank)


def write_if_changed(path: Path, data: bytes) -> bool:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_bytes() == data:
        return False
    path.write_bytes(data)
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--capacity", type=int, default=8 * 1024 * 1024)
    args = parser.parse_args()

    paths = sorted(args.input.glob("*.wav"), key=lambda path: path.name.casefold())
    prepared = [prepare_sample(path) for path in paths]
    bank = build_bank(prepared, args.capacity)
    changed = write_if_changed(args.output, bank)

    for path, sample in zip(paths, prepared):
        duration = len(sample.pcm) / SAMPLE_RATE
        print(
            f"{path.name}: {sample.source_channels}ch {sample.source_bits}-bit "
            f"{sample.source_rate} Hz -> mono PCM8 {SAMPLE_RATE} Hz, "
            f"{duration:.3f}s, peak {sample.peak}, {len(sample.pcm)} bytes"
        )
    print(
        f"Bank: {args.output} ({len(prepared)} samples, "
        f"{len(bank)} bytes, {'updated' if changed else 'unchanged'})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

