# Lichii Console visual QA

## Reference sources

The Switch 1 HOME documentation and its 1280 × 720 archived HOME screenshot
show square game artwork and a separate row of circular system shortcuts.
The theme and software options guides describe how those actions are reached.
The fan projects are secondary interaction references. One demonstrates the
utility row, and the other documents a fixed 16:9 stage and carousel input.
Lichii uses its own layout dimensions, assets and palette. Nintendo screenshot
rights remain with their owners. [Nintendo HOME Menu](https://en-americas-support.nintendo.com/app/answers/detail/a_id/22308) · [Archived HOME screenshot](https://www.mariowiki.com/File:Switch_Home_Menu.png) · [Nintendo theme guide](https://www.nintendo.com/en-gb/Support/Nintendo-Switch/How-to-Change-the-Theme-1496819.html) · [Nintendo software options guide](https://www.nintendo.com/en-gb/Support/Troubleshooting/How-to-Access-a-Software-s-Menu-Options-on-Nintendo-Switch-1434295.html) · [iqbaladinur/switch-ui](https://github.com/iqbaladinur/switch-ui) · [kirankunigiri/nintendo-switch-web-ui](https://github.com/kirankunigiri/nintendo-switch-web-ui)

## Running Lichii captures

The visual test runner launches the actual Qt Quick executable with a temporary
profile and synthetic NRO metadata. It saves the full window and a stage crop
normalized to 1280 × 720. It also records the logical geometry of named QML
items. Existing repository captures in `horizon-reference/` are historical
Lichii captures. Use the runner to regenerate the current ones. The comparison
program supports masked clock or artwork regions and produces overlays and
difference images between Lichii builds. [Capture and comparison workflow](../tests/horizon_visual/README.md)

| Observed problem | Correction |
|---|---|
| One title centered in earlier captures | The first cover stays at (64, 215), measures 216 × 216, and steps by 240 px. |
| All Software caption overlapped its glyph in the inherited binary | Access All Software from the footer with F2 or the controller’s north face button. |
| System actions looked like wide dashboard buttons in the first continuation pass | System actions are 68 px circles in a centered row. A caption appears on focus or hover. |
| Category still looked focused after moving to settings values | A subdued category marker stays visible while the value pane owns the focus outline. |
| Modal content could use a different logical height on 4:3 windows | Each dialog uses the same 720 px logical height as the stage. The window is letterboxed and dialogs are centered at the same scale. |

The matrix checks 0, 1, 3, 12 and 16 games at 1280 × 720, 960 × 540, and
800 × 600. It exercises shelf and grid scrolling, Settings focus and edits,
software options, confirmations, and the configurable options key. Native
compositor runs exercise SDL virtual gamepads and switching prompts back to
keyboard or mouse. Physical controllers, commercial title launch, Windows
runtime, and the native artwork picker are unverified. The UI test uses local
synthetic artwork and settings. [Tests](../src/lichii_console/console_smoke.cpp)

The circular-shortcut build passed all 15 Xvfb cases and three private Weston
cases. The 800 × 600 window uses an 800 × 450 stage with 75 px above and below.
The geometry check permits up to 2 logical pixels of rounding across window
sizes. Offscreen console and core CTests passed. Weston reported headless
audio format and cursor warnings. The Qt Quick lint run was clean.

The follow-up uses neutral light and dark surfaces and distinct keyboard,
Xbox-style, Nintendo-style, and PlayStation-style button prompts. The
Pegasus theme is an additional reference for preserving the console layout
on non-16:9 screens. [Pegasus switchOS theme](https://github.com/PlayingKarrde/switchOS)

The profile follow-up passed six Xvfb visual cases with 1, 3 and 16 titles
at 1280 × 720 and 800 × 600. The actual captures show the saved avatar,
both palettes, the F3 options prompt, and the avatar gallery with its preview
in a separate column. Smoke tests covered name editing, local image selection,
profile switching, controller prompt families and the library shortcut.

The OpenGL synthetic title completed launch, HOME, Resume, and confirmed Stop
under Xvfb. The private Weston run reached launch and pause, then failed the
resume focus assertion. It also lacked a Vulkan present queue. Native
Hyprland game-window focus, commercial titles, physical gamepads and Windows
remain outside that verification.
