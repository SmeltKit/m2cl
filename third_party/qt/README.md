# Qt notices / Qt 许可声明

M²CL uses Qt **6.10.3**, dynamically linked, under the **GNU Lesser General
Public License version 3 only (LGPLv3)**. Copyright (C) The Qt Company Ltd.
and other contributors. Qt is a trademark of The Qt Company Ltd.

The application uses Qt Core, Gui, Widgets, Network, Concurrent and Svg.
The inventory also covers the macOS platform, style, network, TLS and image
format plugins listed in `components.json`, including the WebP plugin.
Qt Test and Linguist are development tools; Qt QML is not an application
runtime dependency. Optional, platform-dependent code recorded in the SDK's
metadata is included in the notices without claiming it is all active.

## Documents shipped with this build

- `LGPL-3.0.txt` and `GPL-3.0.txt`: complete, unmodified upstream license texts.
  LGPLv3 incorporates terms from GPLv3; both are provided.
- `THIRD-PARTY-NOTICES.txt`: component copyrights and full license texts.
- `original/`: the original license, copyright and notice files from Qt sources.
- `components.json`: SDK-derived component inventory and source verification.
- `sources.json`: exact source archive URLs, sizes and SHA-256 checksums.
- `QT-BUILD-CONFIG.txt`: upstream SDK build configuration from its SBOM.
- `SOURCE.txt`: corresponding source, rebuilding and replacement instructions.

The original project license and section 7 additional terms **do not apply to
Qt or its third-party components**. Those parts retain their own licenses,
including the permissions and conditions in the accompanying texts. Nothing
in M²CL's terms restricts modifying or replacing Qt, relinking this application
with a compatible modified Qt, or reverse engineering for debugging such
modifications as permitted by LGPLv3. No Qt source modifications were made
for this baseline.

## Source and distribution

Use `tools/prepare_source_bundles.py` to prepare separate M²CL and Qt source
archives. Distribute them alongside the corresponding binary, with equivalent
download access, and preserve these notices. Merely including an upstream
source link is not the source distribution method used here. See `SOURCE.txt`.

This checked inventory is for the official Qt 6.10.3 **macOS SDK**. A Windows,
Linux, different-version, differently configured or newly deployed dependency
set needs its own inventory review before distribution. A development build
does not by itself establish a complete standalone package.

Official references: [Qt LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations),
[Qt licensing](https://doc.qt.io/qt-6.10/licensing.html).

## 中文说明

本程序动态链接 Qt 6.10.3，按 LGPLv3 使用。Qt 版权属于 The Qt Company Ltd.
及其他贡献者；其第三方组件保留各自许可。项目的 GPLv3 附加署名条款不适用于
Qt，也不限制用户修改、替换 Qt、使用兼容的修改版重新链接程序，以及为调试
这些修改而进行许可允许的逆向工程。

完整 LGPLv3、GPLv3、第三方声明及原文件随程序提供，也可在“关于 → 许可证与
署名 → Qt”离线查看。Qt 对应源码单独提供，不嵌入启动器；发布二进制时，应将
对应的 Qt 和 M²CL 源码压缩包一同提供，获取条件应与二进制等同。

这份清单核对的是 macOS 上的 Qt 6.10.3 官方 SDK；更换平台、Qt 版本、配置或
部署组件后，需要重新核对。构建和替换方式、源码校验值见 `SOURCE.txt`。
