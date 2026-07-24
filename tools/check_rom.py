#!/usr/bin/env python3
"""Small independent checker for the instrument ROM's GBA header."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


EXPECTED_TITLE = b"AMBGRANULAR"
EXPECTED_GAME_CODE = b"AGRN"
EXPECTED_MAKER_CODE = b"00"
EXPECTED_NINTENDO_LOGO = bytes.fromhex(
    "24 ff ae 51 69 9a a2 21 3d 84 82 0a 84 e4 09 ad "
    "11 24 8b 98 c0 81 7f 21 a3 52 be 19 93 09 ce 20 "
    "10 46 4a 4a f8 27 31 ec 58 c7 e8 33 82 e3 ce bf "
    "85 f4 df 94 ce 4b 09 c1 94 56 8a c0 13 72 a7 fc "
    "9f 84 4d 73 a3 ca 9a 61 58 97 a3 27 fc 03 98 76 "
    "23 1d c7 61 03 04 ae 56 bf 38 84 00 40 a7 0e fd "
    "ff 52 fe 03 6f 95 30 f1 97 fb c0 85 60 d6 80 25 "
    "a9 63 be 03 01 4e 38 e2 f9 a2 34 ff bb 3e 03 44 "
    "78 00 90 cb 88 11 3a 94 65 c0 7c 63 87 f0 3c af "
    "d6 25 e4 8b 38 0a ac 72 21 d4 f8 07"
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("rom", type=Path)
    parser.add_argument("--require-power-of-two", action="store_true")
    args = parser.parse_args()

    data = args.rom.read_bytes()
    errors: list[str] = []

    if len(data) < 0xC0:
        errors.append("ROM is too small to contain a GBA header")
    else:
        if data[0x04:0xA0] != EXPECTED_NINTENDO_LOGO:
            errors.append("Nintendo logo data does not match the GBA boot header")
        title = data[0xA0:0xAC].rstrip(b"\x00 ")
        if title != EXPECTED_TITLE:
            errors.append(f"unexpected title {title!r}")
        if data[0xAC:0xB0] != EXPECTED_GAME_CODE:
            errors.append(f"unexpected game code {data[0xAC:0xB0]!r}")
        if data[0xB0:0xB2] != EXPECTED_MAKER_CODE:
            errors.append(f"unexpected maker code {data[0xB0:0xB2]!r}")
        if data[0xB2] != 0x96:
            errors.append(f"fixed header byte is 0x{data[0xB2]:02x}, expected 0x96")

        expected_complement = (-sum(data[0xA0:0xBD]) - 0x19) & 0xFF
        if data[0xBD] != expected_complement:
            errors.append(
                "header complement is "
                f"0x{data[0xBD]:02x}, expected 0x{expected_complement:02x}"
            )

    if len(data) > 32 * 1024 * 1024:
        errors.append("ROM exceeds the GBA's 32 MiB address space")
    if args.require_power_of_two and (len(data) == 0 or len(data) & (len(data) - 1)):
        errors.append("ROM size is not a power of two")

    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1

    digest = hashlib.sha256(data).hexdigest()
    print(f"ROM: {args.rom}")
    print(f"Size: {len(data)} bytes")
    print(f"Title: {EXPECTED_TITLE.decode()}")
    print(f"Game code: {EXPECTED_GAME_CODE.decode()}")
    print(f"SHA-256: {digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
