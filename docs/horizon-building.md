# Building Lichii Console

Console mode requires dynamic Qt 6.8 or newer with Qt Quick, Quick Controls, Qt Multimedia, and Qt SVG. On Arch or Omarchy, install `qt6-declarative`, `qt6-multimedia`, `qt6-svg`, and `qt6-wayland` along with the desktop prerequisites. See the repository's [dependency list](Deps.md).

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DLICHII_USE_BUNDLED_QT=OFF -DLICHII_CONSOLE=ON -DLICHII_TESTS=ON \
  -DENABLE_WERROR=OFF
cmake --build build --target lichii --parallel 12
build/bin/lichii --console
```

Use `build/bin/lichii --console --fullscreen` for a fullscreen start. F11 toggles fullscreen. `build/bin/lichii --desktop` starts the desktop interface. A console-enabled build opens Console when launched without arguments. Builds with `LICHII_CONSOLE=OFF` continue to open the desktop.

The local build used Clang 23, Ninja, and Qt 6.12. `ENABLE_WERROR=OFF` allows this compiler's warning about the existing `-Wno-error-all` flag.

Arrow keys move focus, Enter confirms, Escape goes back, F1 opens HOME, and F2
opens All Software from HOME. The Menu key opens software options. Choose an
alternate key in Console Settings → Controllers. On SDL gamepads the left stick
or D-pad moves focus. The East and South buttons confirm and cancel by default,
Start opens software options, and Guide opens HOME where supported. Switch the
confirm and cancel positions in the same settings page. Prompts use the last input
device and SDL’s reported face-button labels. [SDL button labels](https://wiki.libsdl.org/SDL3/SDL_GetGamepadButtonLabel)

## Running tests

Configure with `-DLICHII_TESTS=ON` and build `lichii` before running:

```sh
ctest --test-dir build --output-on-failure -R horizon_console
QT_QPA_PLATFORM=wayland build/bin/lichii --console-smoke-test
```

The smoke test uses a temporary Linux profile. The Wayland run also exercises SDL navigation through a virtual gamepad. For a populated library with no game launch, generate and scan the synthetic fixtures:

```sh
fixtures=$(mktemp -d)
python3 tests/horizon/generate_homebrew.py "$fixtures"
QT_QPA_PLATFORM=wayland LICHII_CONSOLE_TEST_LIBRARY="$fixtures" \
  LICHII_CONSOLE_TEST_CAPTURES=/tmp/lichii-console-captures \
  build/bin/lichii --console-smoke-test
```

For a manual game-session check, launch Console, add a game directory, and start a title. Press F1 to bring up the paused HOME window. Use the session controls to resume, then stop the game and confirm that HOME regains focus. The optional `LICHII_CONSOLE_TEST_GAME` points the native Wayland or X11 smoke test at a playable homebrew fixture. The offscreen platform lacks a native presentation surface and skips that scenario.

## Capture the interface

Run the Linux capture matrix against a built binary:

```sh
python3 tests/horizon_visual/check.py capture build/bin/lichii /tmp/lichii-captures \
  --sizes 1280x720 960x540 800x600 --counts 0 1 3 12 16
```

The runner uses Xvfb by default. The `--platform wayland` option uses an
available Wayland compositor. Use the comparison command to inspect masked
differences between two runs. [Visual test workflow](../tests/horizon_visual/README.md)

## Windows

Use the existing MSVC or clang-cl instructions with a dynamic Qt kit that includes Qt Multimedia. Set `LICHII_USE_BUNDLED_QT=OFF` and `LICHII_CONSOLE=ON`. The Windows build-check workflow now builds this mode. CMake passes the console QML directory to `windeployqt` to bundle its imports. Windows runtime, installer, and gamepad testing are pending. [Lichii Windows build](Build.md) · [Qt deployment](https://doc.qt.io/qt-6/windows-deployment.html)
