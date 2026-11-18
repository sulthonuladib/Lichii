# SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

"""Capture and compare real Lichii Console renders from its Qt smoke test."""

import argparse
from fnmatch import fnmatch
import json
import os
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "horizon"))
from generate_homebrew import generate


def window_size(text):
    width, delimiter, height = text.lower().partition("x")
    if not delimiter or not width.isdecimal() or not height.isdecimal():
        raise argparse.ArgumentTypeError("expected WIDTHxHEIGHT")
    size = int(width), int(height)
    if size[0] < 700 or size[1] < 500:
        raise argparse.ArgumentTypeError("the console window minimum is 700x500")
    return size


def nonnegative(text):
    value = int(text)
    if value < 0:
        raise argparse.ArgumentTypeError("expected a non-negative integer")
    return value


def rectangle(text):
    name, separator, bounds = text.partition(":")
    try:
        x, y, width, height = map(int, bounds.split(","))
        if not separator or not name or min(x, y) < 0 or min(width, height) < 1:
            raise ValueError
    except ValueError:
        raise argparse.ArgumentTypeError("expected NAME:X,Y,WIDTH,HEIGHT") from None
    return name, (x, y, x + width, y + height)


def verify_geometry(path, size, reference):
    recorded = json.loads(path.read_text(encoding="utf-8"))
    if tuple(recorded["window"]) != size:
        return f"actual window size {tuple(recorded['window'])} differs from requested {size}"
    stable_names = {
        "homeProfile", "homeProfileAvatar", "homeShelf", "homeAddFolder", "gameTileCover",
        "settingsNavigation", "homeSystemControl", "consoleFooter", "settingsCategories",
        "settingsValues", "allSoftwareGrid", "libraryControlsAction", "searchGamesField",
        "filterFavoritesAction", "sortLibraryAction", "softwareOptionsDialog",
        "softwareOptionDetails", "avatarPickerContent", "profileAvatarGrid", "applyProfileAvatar",
        "actionDialogContent",
    }
    geometry = [
        (item["name"], item["bounds"]) for item in recorded["items"]
        if item["name"] in stable_names or item["name"].startswith("dialogAction_")
    ]
    expected = reference.setdefault(path.name, geometry)
    if [name for name, _ in expected] != [name for name, _ in geometry]:
        return "named layout items differ from the first window size"
    for (name, previous), (_, actual) in zip(expected, geometry):
        if any(abs(a - b) > 2 for a, b in zip(previous, actual)):
            return f"{name} moved in logical coordinates from {previous} to {actual}"
    return None


def capture(args):
    binary = args.binary.resolve(strict=True)
    failed = False
    reference_geometry = {count: {} for count in args.counts}
    with TemporaryDirectory(prefix="lichii-console-fixtures-") as temporary:
        fixtures = Path(temporary)
        for count in args.counts:
            generate(fixtures / str(count), count)

        for size in args.sizes or [(1280, 720)]:
            width, height = size
            for count in args.counts:
                case = args.output.resolve() / f"{width}x{height}" / f"{count:02d}-games"
                case.mkdir(parents=True, exist_ok=False)
                output = case / "screens"
                environment = {
                    **os.environ,
                    "QT_QPA_PLATFORM": {"xvfb": "xcb", "wayland": "wayland"}[args.platform],
                    "QT_QUICK_BACKEND": "software",
                    "QT_SCALE_FACTOR": "1",
                    "LICHII_CONSOLE_TEST_CAPTURES": str(output),
                    "LICHII_CONSOLE_TEST_LIBRARY": str(fixtures / str(count)) if count else "",
                    "LICHII_CONSOLE_TEST_GAME": "",
                }
                environment.pop("LICHII_CONSOLE_TEST_SIZE", None)
                if args.sizes:
                    environment["LICHII_CONSOLE_TEST_SIZE"] = f"{width}x{height}"
                command = [str(binary), "--console-smoke-test"]
                if args.platform == "xvfb":
                    command = ["xvfb-run", "-a", "-s",
                               f"-screen 0 {max(width + 120, 1400)}x{max(height + 80, 800)}x24",
                               *command]

                with (case / "smoke.log").open("w") as log:
                    try:
                        result = subprocess.run(command, env=environment, stdout=log,
                                                stderr=subprocess.STDOUT, timeout=95)
                    except subprocess.TimeoutExpired:
                        print(f"{case}: timed out. See smoke.log.", file=sys.stderr)
                        failed = True
                        continue

                expected = ["home", "home-light", "home-empty", "home-utilities",
                            "settings", "settings-control", "settings-control-light",
                            "all-software", "all-software-light"]
                if count:
                    expected += ["game-options", "game-options-light", "confirmation"]
                if count > 1:
                    expected.append("home-single")
                if args.sizes:
                    expected.append("library-controls")
                    if (output / "avatar-picker.png").is_file():
                        expected += ["profile", "avatar-picker"]
                    if count:
                        expected.append("game-details")
                if args.sizes and count > 5:
                    expected.append("home-scroll")
                if args.sizes and count > 12:
                    expected.append("all-software-scroll")
                missing = [name for name in expected if not (output / f"{name}.png").is_file()]
                if args.sizes:
                    missing += [f"window/{name}" for name in expected
                                if not (output / "window" / f"{name}.png").is_file()]
                    missing += [f"geometry/{name}" for name in expected
                                if not (output / "geometry" / f"{name}.json").is_file()]
                if result.returncode or missing:
                    print(f"{case}: smoke exit {result.returncode}, missing {missing}. "
                          "See smoke.log.", file=sys.stderr)
                    failed = True
                else:
                    print(f"{case}: captured {len(expected)} states")
                    if args.sizes:
                        for name in expected:
                            path = output / "geometry" / f"{name}.json"
                            mismatch = verify_geometry(path, size, reference_geometry[count])
                            if mismatch:
                                print(f"{path}: {mismatch}", file=sys.stderr)
                                failed = True
    return int(failed)


def compare(args):
    from PIL import Image, ImageChops, ImageDraw

    args.output.mkdir(parents=True, exist_ok=False)
    baseline = {path.relative_to(args.baseline) for path in args.baseline.rglob("*.png")}
    candidate = {path.relative_to(args.candidate) for path in args.candidate.rglob("*.png")}
    report = {
        "baseline_only": sorted(map(str, baseline - candidate)),
        "candidate_only": sorted(map(str, candidate - baseline)),
        "screens": {},
    }
    compared = 0
    for relative in sorted(baseline & candidate):
        before = Image.open(args.baseline / relative).convert("RGB")
        after = Image.open(args.candidate / relative).convert("RGB")
        if before.size != after.size:
            report["screens"][str(relative)] = {"sizes": [before.size, after.size]}
            continue

        mask = Image.new("L", before.size, 255)
        for pattern, (left, top, right, bottom) in args.mask:
            if fnmatch(str(relative), pattern):
                ImageDraw.Draw(mask).rectangle((left, top, right - 1, bottom - 1), fill=0)
        diff = Image.composite(ImageChops.difference(before, after),
                               Image.new("RGB", before.size), mask)
        red, green, blue = diff.split()
        changed = ImageChops.lighter(ImageChops.lighter(red, green), blue)
        changed = changed.point(lambda value: 255 if value > args.tolerance else 0)
        counts = {"all": {"changed": changed.histogram()[255], "compared": mask.histogram()[255]}}
        for name, bounds in args.region:
            counts[name] = {
                "changed": changed.crop(bounds).histogram()[255],
                "compared": mask.crop(bounds).histogram()[255],
            }
        report["screens"][str(relative)] = counts

        width, height = before.size
        panel = Image.new("RGB", (width * 2, height * 2))
        panel.paste(before, (0, 0))
        panel.paste(after, (width, 0))
        panel.paste(Image.blend(before, after, 0.5), (0, height))
        panel.paste(diff.point(lambda value: min(value * 3, 255)), (width, height))
        output = args.output / relative
        output.parent.mkdir(parents=True, exist_ok=True)
        panel.save(output)
        compared += 1

    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n",
                                             encoding="utf-8")
    print(f"Compared {compared} capture pairs in {args.output}")
    return int(not compared)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    capture_parser = commands.add_parser("capture", help="run the app and capture its tested states")
    capture_parser.add_argument("binary", type=Path)
    capture_parser.add_argument("output", type=Path)
    capture_parser.add_argument("--counts", nargs="+", type=nonnegative, default=[0, 1, 3, 12, 16])
    capture_parser.add_argument("--sizes", nargs="+", type=window_size, metavar="WIDTHxHEIGHT")
    capture_parser.add_argument("--platform", choices=["xvfb", "wayland"], default="xvfb")
    capture_parser.set_defaults(run=capture)

    compare_parser = commands.add_parser("compare", help="make before/after, overlay and diff panels")
    compare_parser.add_argument("baseline", type=Path)
    compare_parser.add_argument("candidate", type=Path)
    compare_parser.add_argument("output", type=Path)
    compare_parser.add_argument("--mask", action="append", type=rectangle, default=[],
                                metavar="GLOB:X,Y,WIDTH,HEIGHT")
    compare_parser.add_argument("--region", action="append", type=rectangle, default=[],
                                metavar="NAME:X,Y,WIDTH,HEIGHT")
    compare_parser.add_argument("--tolerance", type=nonnegative, default=12)
    compare_parser.set_defaults(run=compare)

    args = parser.parse_args()
    try:
        return args.run(args)
    except (OSError, ImportError) as error:
        print(f"{parser.prog}: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
