# Building M²CL

M²CL is a C++17 / Qt Widgets application. For product status and current
limitations, see [简体中文](../README.md) or [English](../README.en.md).

## Requirements

- CMake 3.21 or newer and a C++17 compiler.
- zlib.
- A matching Qt 5.15+ or Qt 6.5+ SDK with Core, Widgets, Network, Concurrent, Svg and
  LinguistTools; Qt Test is also required when tests are enabled.
- The Qt Image Formats WebP plugin for cover images and resource icons.

Use a complete SDK with the runtime dependencies of its development tools.
For example, the official Qt 6 lupdate tool requires QtQml even though the
launcher itself is a Widgets application and does not link QtQml.

## Configure, build and test

The following example selects Qt 6. Replace `/path/to/Qt` with the matching Qt
installation prefix; Qt 5 instructions are provided below.

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

Qt 5.15.19 (Arch Linux's maintained package) has been compiled and checked with
the offline tests and an offscreen UI startup on Linux x86_64. To select Qt 5.15
explicitly, use its complete SDK and a separate build directory:

```sh
cmake -S . -B build-qt5 -DCMAKE_PREFIX_PATH="/path/to/Qt5" -DMCLAUNCHER_QT_MAJOR=5 -DCMAKE_BUILD_TYPE=Release
cmake --build build-qt5 --config Release --parallel
ctest --test-dir build-qt5 -C Release --output-on-failure
```

Keep both Qt 5 and Qt 6 build/test checks when changing shared code. This Linux
verification does not establish compatibility with every Qt 5.15 patch release,
Windows 7/8.1, x86, graphics drivers or TLS backends. Those require their own
target compiler, runtime deployment and OS checks. The current Qt legal inventory
is specifically for the Qt 6.10.3 macOS SDK and must be reviewed for a Qt 5 release.

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

Language catalogs, project licenses, MD4C notices and Qt legal documents are
embedded and copied alongside the build. The Qt inventory is checked against
the official macOS Qt 6.10.3 SDK; review it when changing the SDK, platform or
deployed plugins. See [Qt notices](../third_party/qt/README.md) and
[source/relink instructions](../third_party/qt/SOURCE.txt).

Prepare the corresponding source companions explicitly, using the three
verified upstream archives listed in `third_party/qt/sources.json`:

```sh
python3 tools/prepare_source_bundles.py --source-cache /path/to/qt-source-cache
```

The default output is the ignored `artifacts/distribution-materials/` directory.
Publish both source ZIP files and checksums alongside the binary built from
that source snapshot, with equivalent access. This command performs no network
requests and does not publish anything. Qt source archives are not added to
the application bundle. The current project has not completed standalone
package, signing or installer validation on every platform.
