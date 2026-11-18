# Lichii Console layout specification

The console uses one 1280 × 720 logical stage. The origin is at the top left.
The stage and each in-app dialog use the smaller of the window-width and
window-height scale factors. The stage is centered in the remaining space.
In-app dialog dimensions use the same logical viewport even when the physical
window has a different aspect ratio. File and folder selection use desktop
dialogs.

## Design tokens

| Token | Light | Dark |
|---|---|---|
| Background | `#f0f0f0` | `#242525` |
| Panel | `#fafafa` | `#303131` |
| Command surface | `#e9e9e9` | `#2b2c2c` |
| Main text | `#252727` | `#f3f3f3` |
| Muted text | `#505858` | `#b5b8b8` |
| Divider | `#cacaca` | `#505252` |
| Focus accent | `#22687a` | `#91d3da` |

The main gutter is 64 px. Theme tokens specify 32 px screen headings, 28 px
HOME section headings, 23 px section titles, 21 px body text, 18 px compact
controls, and 15 px auxiliary text. The font is Noto Sans with Qt’s system
fallback. Keyboard and controller focus use a visible outline, and colors
are shared by both palettes. [Theme tokens](../src/lichii_console/qml/Theme.qml)

## HOME

| Component | X | Y | Logical size | State and behavior |
|---|---:|---:|---|---|
| Identity | 124 | 24 | 44 px high | Lichii wordmark beside the profile |
| Profile | 64 | 24 | 44 × 44 | Opens the active local profile |
| Status | Right edge 1216 | 24–68 | Intrinsic | Clock and available host status |
| Header divider | 64 | 88 | 1152 × 1 | Aligns with the content gutter |
| Games heading | 64 | 111 | 42 px high | Main shelf heading |
| Focused title | 64 | 164 | 1152 × 34 | Shows the focused game title |
| First cover | 64 | 215 | 216 × 216 | Shelf pitch 240 px, independent of count |
| Empty action | 64 | 215 | 216 × 216 | Opens a game-folder picker |
| System shortcuts | Centered | 505 | 68 × 68 per circle | 130 px pitch, caption on focus or hover. |
| Footer | 0 | 664 | 1280 × 56 | Compact contextual prompts aligned to the gutter |

The shelf scrolls horizontally and instantiates visible covers on demand.
HOME prioritizes up to 16 games. All Software shows the whole library.

## Settings and library

| Component | Logical bounds or size | State and behavior |
|---|---|---|
| Settings header | x64–1216, y0–86 | Shows the current settings scope |
| Settings categories | x64–328, y102–646 | Compact categories and group dividers |
| Settings boundary | x346 | Separates category selection from the contents |
| Settings contents | x372–1216 | 60 px minimum row heights, vertically scrollable |
| All Software header | x64–1216 | Search and filter entry and current sort summary |
| All Software grid | Five columns with 204 × 204 covers at 1280 × 720 | Covers, labels, independent scrolling, game-key selection restoration |
| Software options | 640 × 642, centered | Cover and title header, action list, nested information |
| Confirmation | 620 px wide, centered | Height follows content. Destructive actions default to Back. |

Arrows move focus and Enter activates. Escape returns to the previous view
or cancels the active edit. F1 returns to HOME and F2 opens All Software
from HOME. F3 opens software options by default. The options key is configurable in Controllers. Button
prompts reflect the latest input and SDL's reported controller type.

The profile header shows the active local user’s saved avatar. The 848 × 608
avatar picker has a four-column gallery and a separate preview. A user-provided
folder or image can supply avatar choices.
