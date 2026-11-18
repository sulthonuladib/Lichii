# Lichii Console visual testing

Build Lichii with `LICHII_CONSOLE=ON`, `LICHII_TESTS=ON`, and dynamic Qt. The
capture command uses the actual Qt Quick executable, a temporary Lichii
profile, and generated NRO metadata and fallback covers. It requires Python
3 and Xvfb. To use a Wayland session with compositor keyboard focus, pass
`--platform wayland`.

```sh
python3 tests/horizon_visual/check.py capture build/bin/lichii /tmp/lichii-candidate \
  --sizes 1280x720 960x540 800x600 --counts 0 1 3 12 16
```

Each case writes the QtTest log, 1280×720 stage captures, full-window captures,
and geometry from the actual visible QML items. The geometry manifests list
logical item bounds, focus, and stage placement in the window. The smoke test
checks the scan count, scale, HOME cover placement, carousel and grid scroll,
settings navigation and edits, confirmation focus, and a virtual SDL gamepad
when the compositor gives the test window focus. The gamepad run checks generic,
PlayStation and Switch Pro button labels, confirm/back swapping, and switching
back to keyboard prompts after a mouse click. Across sizes, the capture command
checks major item positions within two logical pixels. A failure or missing
capture makes the command fail after running the remaining cases.

For a previously built Lichii binary that predates window size overrides, omit
`--sizes` to capture the old build at its default window size. Use an output
directory for each run. The comparison command uses Pillow to create one
panel per matching screenshot. The quadrants show the baseline, candidate,
their 50% overlay, and a contrast-enhanced difference image.

```sh
python3 tests/horizon_visual/check.py compare \
  /tmp/lichii-baseline/1280x720/03-games/screens \
  /tmp/lichii-candidate/1280x720/03-games/screens /tmp/lichii-comparison \
  --mask 'home*.png:1050,0,230,80' --region 'covers:0,190,1280,280'
```

Set rectangles to suit the captures being reviewed. A mask excludes the named
area from the report and difference quadrant, for example a clock or profile
image. The report lists changed pixels by region and missing screenshot pairs.
Use the geometry manifests and inspect the original renders to assess alignment
and legibility. The difference count measures change and carries no pass score.
Keep captures of personal game libraries local.
