# SPDX-FileCopyrightText: Copyright 2026 Lichii contributors
# SPDX-License-Identifier: GPL-3.0-or-later

"""Generate the built-in console PCM WAVs using only Python's standard library."""

from math import exp, sin, tau
from pathlib import Path
from struct import pack
from typing import NamedTuple
from wave import open as open_wave


SAMPLE_RATE = 48000


class Note(NamedTuple):
    start_ms: int
    length_ms: int
    pitch: float
    gain: float
    bend: float = 0.0
    attack_brightness: float = 0.25


SOUNDS = {
    "focus": [Note(0, 28, 875, 0.44, 0.24, 0.67)],
    "confirm": [Note(0, 43, 640, 0.54, 0.08, 0.43), Note(23, 68, 1016, 0.56, -0.09, 0.36)],
    "cancel": [Note(0, 40, 528, 0.44, 0.10, 0.41), Note(18, 55, 346, 0.49, 0.16)],
    "open": [Note(0, 73, 392, 0.43, 0.10), Note(30, 90, 588, 0.46, -0.09, 0.18)],
    "options": [Note(0, 36, 756, 0.53, 0.18, 0.58), Note(19, 64, 504, 0.44, 0.07)],
    "warning": [Note(0, 79, 235, 0.62, 0.18), Note(94, 84, 235, 0.52, 0.13)],
    "home": [Note(0, 72, 392, 0.43), Note(29, 78, 523, 0.40), Note(57, 102, 659, 0.49)],
}


def voice(seconds, duration, note):
    attack = min(seconds / 0.0012, 1.0)
    release = min((duration - seconds) / 0.007, 1.0)
    envelope = attack * release * exp(-3.6 * seconds / duration)
    bend_time = 0.006
    bend = note.bend * bend_time * (1 - exp(-seconds / bend_time))
    phase = tau * note.pitch * (seconds + bend)
    transient = note.attack_brightness * exp(-seconds / 0.004)
    body = 0.86 * sin(phase) + 0.14 * sin(2 * phase)
    click = 0.42 * sin(3.17 * phase) + 0.19 * sin(5.24 * phase)
    return note.gain * envelope * (body + transient * click)


def render(notes):
    length_ms = max(note.start_ms + note.length_ms for note in notes)
    samples = [0.0] * round(length_ms * SAMPLE_RATE / 1000)
    for note in notes:
        offset = round(note.start_ms * SAMPLE_RATE / 1000)
        duration = note.length_ms / 1000
        for frame in range(round(duration * SAMPLE_RATE)):
            samples[offset + frame] += voice(frame / SAMPLE_RATE, duration, note)
    return pack(f"<{len(samples)}h", *(round(sample * 32767) for sample in samples))


def main():
    directory = Path(__file__).resolve().parent / "sounds"
    directory.mkdir(exist_ok=True)
    for name, notes in SOUNDS.items():
        with open_wave(str(directory / f"{name}.wav"), "wb") as output:
            output.setparams((1, 2, SAMPLE_RATE, 0, "NONE", "not compressed"))
            output.writeframes(render(notes))


if __name__ == "__main__":
    main()
