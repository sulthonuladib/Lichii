# Lichii Console architecture

Lichii Console is an optional Qt Quick interface in the existing `lichii` executable. HOME uses a horizontal shelf of square covers and labeled circular shortcuts. All Software and System Settings have dedicated pages. The Qt Quick stage and its action panels use a shared 1280 × 720 coordinate space, scaled uniformly to the window. The presentation uses Lichii’s light and dark palettes, blue-green focus, and original icon set. [Visual QA and references](horizon-visual-fidelity.md)

## Process and rendering

`src/lichii_console` owns the Qt Quick resources and the `Horizon::ConsoleApp` interface. The desktop `MainWindow` owns the emulator, applets, configuration dialogs, render widget, and the single `Core::System`. It starts hidden in Console mode. Starting a title calls the existing in-process boot path, including per-title settings resolution, shader preparation, and applet setup. The native render widget remains responsible for the Vulkan or OpenGL surface.

F1 and the SDL Guide button pause an active game and bring up the Qt Quick window. The application keeps the native render surface alive and hands focus back to it on Resume. The two windows are managed by the compositor. The pause screen does not require sharing render resources with Qt Quick. Loading and shutdown use the existing desktop progress and error dialogs. Boot preparation still has synchronous steps that require the GUI thread, in addition to the existing background ROM preparation and emulation thread.

The desktop interface can be opened from Console while idle and returned to Console with F1 or by closing the desktop window. Both use the same in-memory settings and models.

## Library, profiles, and settings

The console list model adapts the desktop's asynchronous `GameListWorker` and `GameListModel`. Both interfaces consume their game-directory configuration, title metadata, and extracted artwork. HOME ordering is independent of All Software filters. Separate persistent entries for recency, no-ID favorites, and no-ID artwork use the existing Lichii config/data paths. Games with program IDs share the desktop's favorites and custom artwork. Program IDs identify overrides and display names are free to change.

The console settings bridge exposes a curated set of validated Lichii settings. It writes global values through `QtConfig`, and per-game values through the same `custom/<program-id>.ini` implementation used by the desktop. It restores the global view after an override is read or written. Core settings are editable while idle. Controller assignment, physical mapping, input profiles, and local profile management use the existing Settings and Properties dialogs. The HOME navigation mapping uses SDL's gamepad abstraction, with configurable confirm/cancel face-button placement and a configurable keyboard options key. The footer reflects the latest input and uses SDL’s reported controller buttons.

Console-only preferences live in `console.ini` under Lichii's config directory, including portable installs. Packs are local directory overrides for original assets already included in the application. [Asset manifest](horizon-assets.md)
