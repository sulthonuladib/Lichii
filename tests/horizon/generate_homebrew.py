# SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

"""Generate synthetic NROs for native console frontend smoke tests."""

import argparse
from pathlib import Path
from struct import pack_into


def generate(directory: Path, count: int = 6) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    titles = (
        "Signal Valley", "Aster Drift", "Hollow Fern", "Paper Moon", "Windward", "Evenfall"
    )
    for index in range(count):
        title = titles[index] if index < len(titles) else f"Signal Archive {index + 1:02d}"
        program = bytearray(0x3000)
        pack_into("<II4s4s4sIII", program, 0, 0x14000020, 0xA0, b"HOME", b"BREW",
                  b"NRO0", 0, len(program), 0)
        pack_into("<7I", program, 0x20, 0, 0x1000, 0x1000, 0x1000, 0x2000, 0x1000, 0)
        # The guest repeatedly sleeps for 100 ms so the host can test lifecycle transitions.
        pack_into("<4IQ", program, 0x80, 0x58000080, 0xD4000161,
                  0x17FFFFFE, 0xD503201F, 100_000_000)

        assets = bytearray(0x38)
        pack_into("<4sI6Q", assets, 0, b"ASET", 0, 0, 0, len(assets), 0x4000, 0, 0)
        nacp = bytearray(0x4000)
        for language in range(16):
            slot = 0x300 * language
            name = title.encode()
            nacp[slot:slot + len(name)] = name
            nacp[slot + 0x200:slot + 0x20A] = b"Lichii QA "
        pack_into("<I", nacp, 0x302C, 0xFFFF)
        nacp[0x3060:0x3065] = b"1.0.0"
        pack_into("<Q", nacp, 0x3078, 0x0100123456700000 + index * 0x2000)
        (directory / f"{title}.nro").write_bytes(program + assets + nacp)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--count", type=int, default=6, help="number of fixtures to generate")
    args = parser.parse_args()
    if args.count < 0:
        parser.error("--count must be non-negative")
    generate(args.directory, args.count)
