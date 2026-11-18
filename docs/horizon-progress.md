# Lichii Console progress

| Milestone | Status |
|---|---|
| 0. Audit | Created `feature/horizon-console` from the clean `nx2` checkout at `8c457aba08`. Configured and built the desktop baseline and core test binary with Clang and Ninja. |
| 1. Qt Quick shell | Built and ran the optional frontend on Omarchy in windowed and fullscreen modes. |
| 2. Real library | Verified empty and populated scans, missing folders, metadata, search, sort, favorites, and managed artwork import and reset. |
| 3. Settings | Connected curated global and per-game settings to QtConfig. Checked that a fresh Lichii process reads the saved global, per-game, and appearance preferences. Reused the existing controller, profile, and per-game input editors. |
| 4. Game lifecycle | Wired launch, failure recovery, pause, resume, stop, and return to HOME through the in-process emulator. A synthetic homebrew title completed a lifecycle smoke run. The user's play, close, and return-to-Console check is pending. |
| 5. Visual and audio | Reworked HOME around the software shelf and circular shortcuts. All Software and Settings use dedicated pages, and options and confirmations use standalone panels. Replaced the original cue set with shorter synthesized sounds. Keyboard focus and the virtual SDL gamepad passed native Wayland checks. |
| 6. Launch modes | Console-enabled builds accept `--console` and `--desktop`. The unqualified command opens desktop mode while game handoff is being validated. |
| 7. In-game HOME | Implemented a separate Qt Quick pause window. F1 and Guide handoff, sustained gameplay rendering, and focus restoration with a real title are pending manual validation. |
| 8. Portability | Added Linux smoke coverage and enabled compilation in the existing Windows CI job. Windows execution, physical controllers, and macOS feasibility remain open. |

The implementation ran in parallel across the QML interface, game library and metadata, appearance and assets, and C++ integration and QA. The captures show the current Qt Quick UI on Wayland with synthetic title metadata and built-in artwork.

The core test suite, offscreen console CTest, QML lint, and native Wayland console smoke test passed. The final smoke runs left game launch disabled. The Windows workflow has not been run locally.

[HOME capture](horizon-reference/home.png) · [Settings capture](horizon-reference/settings.png) · [Visual QA](horizon-visual-fidelity.md)

[Architecture](horizon-architecture.md) · [Build and verification](horizon-building.md) · [Assets](horizon-assets.md)

## Continuation

| Milestone | Implementation |
|---|---|
| HOME and themes | Single logical left-anchored shelf, compact profile and status, circular system shortcuts, and shared light/dark tokens. |
| Settings | Flat category and value panes with isolated focus and editable sliders. Choice dialogs open on the saved value. |
| All Software and options | Five-column grid, search/sort/favorites, nested game information, retained artwork and per-game configuration commands. |
| Dialogs and resolution | In-app panels and the page stage share the same 1280 × 720 scale. Native file pickers follow the desktop. |
| Input | Configurable options key, focus history, clickable footer actions, and controller prompts derived from SDL's button labels. |
| Launch modes | With the console compiled in, Lichii without arguments opens it. Desktop mode is available with `--desktop`. |
| Visual QA | QtQuick capture and geometry workflow in [tests/horizon_visual](../tests/horizon_visual/README.md). See the [verification record](horizon-visual-fidelity.md). |

## Profile, prompts and game handoff

- F3 is the default software-options key. F2, the clickable footer, and the
  controller’s north face button open All Software from HOME.
- HOME shows the active profile picture. The profile menu can create, rename,
  switch, and set an avatar from original presets or local images.
- Console launch uses its own status screen. Guide pauses and brings up HOME. Guide
  again opens Resume and Stop. Wayland keeps the paused game surface mapped.
