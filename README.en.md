<picture>
  <source media="(prefers-color-scheme: dark)" srcset="assets/branding/readme-banner-dark.svg">
  <source media="(prefers-color-scheme: light)" srcset="assets/branding/readme-banner-light.svg">
  <img src="assets/branding/readme-banner-light.svg" width="100%" alt="M²CL · Mercury MC Launcher — Cross-platform Minecraft: Java Edition launcher">
</picture>

<p align="center"><a href="README.md">简体中文</a> · <strong>English</strong></p>

An independent desktop launcher for **Minecraft: Java Edition**, built with **C++17 and Qt Widgets**.
M²CL organizes Java runtimes, mods, shaders and resource packs around individual game instances,
with a forest-green interface and light, dark and system appearance modes.
The project targets multiple platforms and architectures, spanning older and newer Windows systems, macOS and Linux. Performance and compatibility are still being evaluated.

> **0.1.0 early development preview.** This version is intended for trying features and providing feedback. The interface, features, configuration format and platform coverage may still change substantially.
> Larger updates are planned as core features mature and Microsoft / Minecraft API access review and real-account validation progress. Their timing and scope are not yet determined.
> Microsoft-account authentication has been implemented, but real sign-in currently receives HTTP 403 from Minecraft Services.
> API access for this application's own client ID has not been confirmed. This is not an approved release or a claim of working end-to-end online sign-in.
> The current development build also includes offline-player launch without Microsoft authentication; its release authentication and ownership requirements still need to be revised before review.

## Current features

| Area | Implemented scope |
| --- | --- |
| Game instances | Vanilla installation, existing-directory discovery, instance-specific settings, favorites, custom names and covers |
| Java | Quick detection, cancelable disk scanning, major version / distribution / build selection, a separate runtime choice for each instance |
| Launch settings | Recommended or manual memory, initial heap, window dimensions, fullscreen, additional JVM and game arguments |
| Loaders | Fabric, Quilt, modern Forge and NeoForge; OptiFine managed as a separate version component |
| Mods | Modrinth browsing, local imports, icons and versions, batch enable / disable, recursive dependency plans, duplicate detection and static conflict checks |
| Shaders and resource packs | Separate market entries, per-instance imports and installation, resource lists, shader support components and installation confirmation |
| Downloads and logs | Official / BMCLAPI source selection for mapped resources, hash verification, cancellation, cache reuse, explicit errors and manual retry, game logs |
| Accounts | Offline-player management; Microsoft browser PKCE, Xbox / Minecraft entitlement and profile verification, system credential stores and captured-log redaction; live validation is incomplete |

Checks are specific to game, loader, mod, runtime and shader versions. Passing static checks does not guarantee that every combination will run.

## Platforms and languages

The following are **planned platform and architecture targets**. Compatibility validation is ongoing:

| Platform | Target architectures | Current validation |
| --- | --- | --- |
| Windows 7 SP1 / 8 / 8.1 | x86 (32-bit), x64 (64-bit) | Qt 5 compatibility path; execution on the target systems and architectures remains to be validated |
| Windows 10 | x86 (32-bit), x64 (64-bit) | Windows x64 build, test and selected installation results are recorded; individual OS versions and x86 still require validation |
| Windows 11 | x64, ARM64 | Windows x64 build, test and selected installation results are recorded; native ARM64 builds and execution remain to be validated |
| macOS | Intel x64, Apple Silicon ARM64 | ARM64 builds run and selected game combinations reached client initialization; Intel and the minimum OS version remain to be validated |
| Linux | x64; ARM64 as an additional target | x64 builds and offline tests passed with Qt 5 and Qt 6; distribution coverage, graphical gameplay and ARM64 remain to be validated |

The launcher, Java, Minecraft and loaders have separate compatibility requirements; targeting older systems and 32-bit hardware does not imply that every game version will run.
Complete gameplay and Microsoft sign-in validation remain pending.

Choose the interface language under Settings → Appearance and restart the launcher to apply it.
The default follows the system; unsupported system languages fall back to English. Project descriptions, instance names, version identifiers and game logs retain their original text.
See the [translation guide](docs/internationalization.md).

## Build and run

Requirements: CMake 3.21+, a C++17 compiler, zlib, and **Qt 5.15+ or Qt 6.5+** with Core, Widgets, Network, Concurrent and Svg.
LinguistTools is required to compile translations; tests also require Qt Test. Deploy the Qt Image Formats WebP plugin to read the bundled WebP covers.

The example below uses Qt 6. Replace `/path/to/Qt` with your Qt SDK installation prefix.
For Qt 5, select the matching Qt 5 SDK and change `MCLAUNCHER_QT_MAJOR=6` to `MCLAUNCHER_QT_MAJOR=5`.

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="/path/to/Qt" -DMCLAUNCHER_QT_MAJOR=6 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Typical executable locations:

- macOS: `build/m2cl.app/Contents/MacOS/m2cl`
- Linux: `build/m2cl`
- Windows: `build/Release/m2cl.exe`, or `build/m2cl.exe` with a single-configuration generator

Qt 5.15.19 has been built and checked with offline tests and a UI startup check
on Linux x64. Windows 7/8.1 and x86 still require validation with their target
compiler, Qt package and operating system.
Standalone deployment, signing and installers are also incomplete. Copying the executable alone does not deploy its Qt dependencies and plugins.
See the [build guide](docs/build-and-validation.md) for testing and deployment notes.

## Data, services and accounts

- Settings use the platform's application configuration directory; `--data-dir` selects an isolated directory. Runtime and launch settings belong to each instance.
- Java downloads support Temurin, Zulu, Microsoft OpenJDK and Oracle OpenJDK. Minecraft's official runtime channel is used separately for automatic provisioning.
- The resource market currently uses Modrinth. CurseForge integration and an application-owned API key are planned after the first complete version.
- Microsoft sign-in uses this project's own public client ID. API access and live sign-in validation remain pending.
  Refresh credentials are kept out of ordinary settings and saved through the OS credential store; session-only use is available explicitly when storage is unavailable.
- Built-in version covers are AI-generated thematic illustrations, not gameplay screenshots. See [cover assets](assets/covers/README.md) for provenance and packaging notes.

## Next steps

- Prepare authentication and game-entitlement enforcement for official review, request API access, and validate real sign-in and launching.
- After the first complete version, apply for a project-owned CurseForge API key and add resource search, details, version filtering and downloads.
- Add per-instance world management: world lists and version information, imports / exports, copying, backups and restoration.
- Continue reviewing terminology, long text and Windows / Linux font layout across all three languages.
- Expand mod / shader compatibility checks, legacy-loader support and Windows / Linux graphical validation.
- Add modpack imports, download pause / persistent resume, standalone packages and performance measurements.

## Contributing

- [Building and testing](docs/build-and-validation.md)
- [Translation guide](docs/internationalization.md)

## License and attribution

Copyright (C) 2026 **ppm203**.

Original project source code is licensed under **GNU GPL version 3 only, supplemented by section 7 additional terms**:
[full license](LICENSE), [attribution and modified-version terms](ADDITIONAL_TERMS.md), and [copyright / third-party notices](NOTICE).
Redistributed modifications must preserve reasonable attribution, accurately identify their origin and reasonably distinguish themselves as third-party modified versions rather than releases endorsed or supplied by the original author.
The additional terms do not prohibit forks, commercial distribution or advertising, or require a fixed logo, hyperlink or UI layout.

Third-party components retain their own licenses; MD4C's original MIT notice is preserved [here](third_party/md4c/LICENSE.md).

Qt is dynamically linked under LGPLv3; see the [Qt legal materials](third_party/qt/README.md).
The About screen provides offline license access. Binary releases must include
the separate corresponding Qt and M²CL source companions described in the
[distribution instructions](docs/build-and-validation.md#distribution).
M²CL is an independent project and is not affiliated with or endorsed by Mojang Studios or Microsoft.
