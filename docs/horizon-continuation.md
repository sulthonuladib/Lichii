# Lichii Horizon continuation

Handoff audit for the Qt Quick frontend, taken on branch `feature/horizon-console`
at `8c457aba08` with the console work uncommitted.

## Current functionality

| Area | State | Source |
|---|---|---|
| Console process mode | `lichii --console` and `--console [--fullscreen]`; F11 toggles fullscreen; `--desktop` opens the Qt Widgets UI. | `src/lichii/main.cpp` |
| Qt Quick shell | 1280 × 720 logical stage scaled uniformly and centred in an `ApplicationWindow`. | `src/lichii_console/qml/Main.qml` |
| Game library | Asynchronous scan reuses `GameListWorker`; size/favorites/search/sort, per-game artwork, managed covers, folder add/remove, deep scan. | `src/lichii_console/library_model.{h,cpp}` |
| HOME | Horizontal shelf, profile capsule, clock, All Software button, five labelled circular shortcuts, large footer. | `qml/HomePage.qml` |
| All Software | Six-column grid or list, caption line, selected-title line, controls sheet for search/filter/sort/view, options button. | `qml/LibraryPage.qml` |
| Settings | Two-pane categories and settings, per-game categories, appearance, real QtConfig bindings, controller/profile editors. | `qml/SettingsPage.qml`, `src/lichii_console/console_settings.cpp` |
| Options / confirmations | Modal `ActionDialog`; sheet for software options, centred panel for confirmations. | `qml/ActionDialog.qml`, `qml/PanelDialog.qml` |
| Input | Keyboard focus chain, SDL virtual gamepad polling, configurable confirm/cancel face buttons, focus history. | `src/lichii_console/console_app.cpp`, `qml/Main.qml` |
| Audio | Synthesized focus/confirm/cancel/open/options/warning/home cues; user pack override. | `src/lichii_console/appearance.cpp`, `assets/sounds` |
| Tests | Offscreen `horizon_console_smoke` CTest plus real Qt Quick captures via `LICHII_CONSOLE_TEST_CAPTURES`. | `src/lichii_console/console_smoke.cpp` |

## Relevant paths

- `src/lichii_console/qml/` — all console QML, 13 components.
- `src/lichii_console/assets/` — original SVG icon set, fallback cover, WAV cues.
- `src/lichii_console/console_app.{h,cpp}` — `ConsoleApp` QML singleton (`consoleApp`), library/appearance ownership, launch lifecycle, SDL navigation.
- `src/lichii_console/console_settings.cpp` — curated global/per-game settings bridge.
- `src/lichii_console/library_model.cpp` — console library adapter and artwork storage.
- `src/lichii_console/console_smoke.cpp` — navigation, controller, settings readback, lifecycle and screenshot harness.
- `CMakeLists.txt`, `src/lichii/CMakeLists.txt` — `LICHII_CONSOLE`, Qt Quick resources, CI.
- `docs/horizon-architecture.md` — process and backend architecture.
- `docs/horizon-reference/` — Lichii's own captures (not Nintendo references).
- `/tmp/lichii-switch1-references/` — local reference captures used for measurement; not redistributed.

## Existing UI structure

`ApplicationWindow` → fixed `stage` (1280 × 720) → page stack (`HomePage`,
`LibraryPage`, `ScreenshotsPage`, `SettingsPage`) above a 60 px shared footer.
`Theme.qml` is a `QtObject` of tokens. Shared controls are `NavButton`, `Legend`,
`GameTile`, `SettingRow`, `PanelDialog`, `ActionDialog`.

## Build and capture method

Incremental build (already configured):

```sh
cmake --build /tmp/lichii-horizon-build --target lichii --parallel 20
QT_QPA_PLATFORM=offscreen LICHII_CONSOLE_TEST_CAPTURES=/tmp/horizon-captures \
  /tmp/lichii-horizon-build/bin/lichii --console-smoke-test
```

Synthetic library:

```sh
fixtures=$(mktemp -d)
python3 tests/horizon/generate_homebrew.py "$fixtures"
QT_QPA_PLATFORM=offscreen LICHII_CONSOLE_TEST_LIBRARY="$fixtures" \
  LICHII_CONSOLE_TEST_CAPTURES=/tmp/horizon-captures \
  /tmp/lichii-horizon-build/bin/lichii --console-smoke-test
```

Captures are real `QQuickWindow::grabWindow()` output cropped to the logical
stage and scaled to 1280 × 720.

## Known UI problems at handoff

1. HOME shelf is centred; one game is centred instead of left-anchored at
   reference `x = 107`.
2. Selected title is a large centred heading with publisher/version beneath.
   Reference shows a left-aligned cyan title above the shelf only while a game
   is focused.
3. Colours are porcelain/ink with an indigo accent. Reference uses `#ebebeb`
   (`#2d2d2d` dark) and turquoise focus with a sky-blue title.
4. Profile is a rounded name capsule; reference uses a 64 px avatar at
   `(56, 32)`.
5. Status is a large clock only; reference has a compact clock, Wi-Fi and
   battery cluster at the upper right.
6. All Software is a floating header button; reference puts a square
   "All Software" tile at the end of the shelf.
7. System shortcuts are 59 px labelled circles with permanent captions;
   reference uses an 86 px circle row (`gap 22`) with a caption only on focus.
8. Footer is a 60 px coloured rail with persistent key legends; reference has
   a 1 px divider at `y = 647` and minimal contextual prompts.
9. Settings has a large rounded Back button, purple category pills, a tinted
   sidebar, content headings and dashboard-style rows. Reference has a flat
   `#f0f0f0` sidebar to `x = 409`, categories at `x = 75` with 80 px focus
   boxes, content from `x = 470` to `1190`, 91 px rows and a flat header.
10. All Software has a back button, selected-title line and captions; reference
    has a 4-square header, sort label, 174 px tiles with 10 px gaps starting at
    `(93, 166)` and no captions.
11. Software options use a 470 px side sheet rather than the original
    full-height two-column panel.

## Known runtime / backend limits

- Game launch lifecycle is wired, but the final real-title smoke was not run.
- In-game F1 pause window and focus handoff need a real game.
- Physical controller feel and Windows runtime are untested.
- The offscreen capture platform avoids compositor timing differences.
- No Nintendo firmware, keys, fonts or assets are present or redistributed.

## Immediate priorities

1. Replace tokens and HOME geometry; verify with the real capture harness.
2. Apply the same geometry to Basic Black.
3. Rebuild Settings two-pane geometry on top of the existing bindings.
4. Rebuild All Software grid geometry; keep search/sort/favorites/artwork.
5. Restyle confirmations; then software options.
6. Re-run smoke, captures and QML lint; update fidelity documents.

## Continuation checkpoint

The working tree contains the previous agent’s tracked integration changes and
untracked console sources, tests, documentation and captures. This work continues
on that tree without a checkpoint commit. The opening audit above describes the
previous rendered build. The source at handoff already contains a left-anchored
HOME shelf draft in `HomePage.qml`, measured tokens in `Theme.qml`, compact footer
in `Main.qml`, and host reachability and battery reporting in `ConsoleApp`.
The current CMake cache is `/tmp/lichii-horizon-build` with Clang, Ninja,
`LICHII_CONSOLE=ON`, and `LICHII_TESTS=ON`.

The continuation keeps the console interaction model and uses Lichii’s own visual
design. Priorities are stable sparse-library navigation, a compact HOME command
rail, focus across settings panes and modals, and artwork/settings persistence.
Nintendo’s public screenshots document the console interaction precedent. The
current visual target and geometry are Lichii’s, with regression captures taken
from the actual Qt Quick app.

## Current implementation

HOME uses a left-anchored square-cover shelf, a profile at the upper left,
compact status at the upper right, and a centered circular system row.
All Software is accessible through the footer and shortcuts. Settings supports separate category
and value focus, keyboard slider adjustment, and mouse track selection.
All Software keeps search, sorting, favorites, game details, and per-title
actions. Dialogs follow the fixed stage scale. Control prompts update for
keyboard, mouse, or the active SDL gamepad.

The current build passed the 15-case Linux capture matrix, with three
additional private Weston runs. Physical controller, commercial game, native
artwork picker, and Windows runtime coverage remain pending.
[Geometry](horizon-layout-spec.md) · [Visual checks](horizon-visual-fidelity.md) · [Build](horizon-building.md)
