# Building M²CL

M²CL is a C++17 / Qt Widgets application. For product status and current
limitations, see [简体中文](../README.md) or [English](../README.en.md).

## Requirements

- CMake 3.21 or newer and a C++17 compiler.
- zlib.
- A matching Qt 6.5+ SDK with Core, Widgets, Network, Concurrent, Svg and
  LinguistTools; Qt Test is also required when tests are enabled.
- The Qt Image Formats WebP plugin for cover images and resource icons.

Use a complete SDK with the runtime dependencies of its development tools.
For example, the official Qt 6 lupdate tool requires QtQml even though the
launcher itself is a Widgets application and does not link QtQml.

## Configure, build and test

Replace `/path/to/Qt` with your Qt installation prefix:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="/path/to/Qt" -DMCLAUNCHER_QT_MAJOR=6 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Common executable locations:

| Platform / generator | Executable |
| --- | --- |
| macOS | `build/m2cl.app/Contents/MacOS/m2cl` |
| Linux / single-configuration | `build/m2cl` |
| Windows / multi-configuration | `build/Release/m2cl.exe` |

A Qt 5.15 selector remains in the build configuration, but that toolchain is
not validated. Use separate build directories for different SDKs or compilers.

Tests include local fixtures and simulated service responses. Some require a
loopback TCP port or the system Trash / Recycle Bin. Qt UI tests run offscreen.
Python, if available, enables translation-catalog validation. These checks do
not establish working live Microsoft sign-in or complete gameplay compatibility.

## Isolated UI checks

`--data-dir DIRECTORY` selects a separate launcher configuration directory.
Game and runtime paths still come from that configuration; this is not a
filesystem sandbox. `--smoke-test` renders an offline UI state and exits.
`--language en_US`, `zh_TW`, `zh_CN` or `system` overrides the language for
one run without changing the saved language preference.

Synthetic UI fixtures and preview tooling are in `tools/`. Fixtures contain
metadata for layout checks and are not playable game installations. Live
installation probes under `tests/` are opt-in targets and may download files;
use a dedicated test directory when running them.

## Distribution

A development executable is not a standalone distribution. Deploy the matching
Qt libraries, platform plugins, image formats and their dependencies; verify
that WebP and SVG work on a machine without the development SDK.

Language catalogs are compiled into the application. Project licenses and
MD4C notices are embedded and copied alongside the build. Preserve applicable
licenses, third-party notices and corresponding source obligations when
preparing a release. The current project has not completed standalone package,
signing or installer validation on every platform.
