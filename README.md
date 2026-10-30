# M²CL · Mercury MC Launcher

**简体中文** | [English](README.en.md)

<img src="assets/branding/m2cl-128.png" width="80" height="80" alt="M²CL 图标">

一个使用 **C++17 / Qt Widgets** 开发的独立 Minecraft: Java Edition 桌面启动器。
围绕游戏实例管理 Java、模组、光影和材质包，采用森林绿界面，支持浅色、深色和跟随系统。
项目面向低配电脑设计，性能与兼容性仍在验证中。

> **0.1.0 开发预览。** 界面支持简体中文、繁体中文和英文，默认跟随系统；语言设置重启后生效。
> 微软账号流程已接入，但真实登录目前在 Minecraft 服务接口遇到 HTTP 403；
> 本项目的 API 访问许可尚未确认，不能视为已支持可用的正版登录或已通过官方审核。
> 当前开发版仍有无需微软登录的离线角色入口，送审发布版的身份与游戏资格门槛尚待调整。

## 已有功能

| 方向 | 当前实现 |
| --- | --- |
| 游戏实例 | 原版安装、已有目录识别、独立设置、收藏、自定义名称与封面 |
| Java | 快速检测、可取消的全盘扫描、版本 / 发行版 / 构建选择，每个实例独立指定运行时 |
| 启动设置 | 自动推荐或手动分配内存、初始内存、窗口尺寸、全屏、附加 JVM / 游戏参数 |
| 加载器 | Fabric、Quilt、现代 Forge / NeoForge；OptiFine 作为独立版本组件管理 |
| 模组 | Modrinth 浏览、本地导入、图标与版本、批量启停、递归依赖计划、重复文件识别、静态冲突预检 |
| 光影与材质包 | 独立市场入口、按实例导入与安装、资源列表、光影支持组件与安装确认 |
| 下载与日志 | 已映射资源的官方 / BMCLAPI 来源选择、哈希校验、取消、缓存复用、明确错误与手动重试、游戏日志 |
| 账号 | 离线角色管理；微软浏览器 PKCE、Xbox / Minecraft 资格与角色校验流程、系统凭据库及认证日志脱敏；实网验收未完成 |

加载器、模组、Java 和光影均按具体版本检查；静态检查通过不代表所有组合都能运行。

## 平台与语言

| 平台 | 开发验证状态 |
| --- | --- |
| macOS ARM64 | 已构建和运行；部分原版 / 加载器组合完成客户端初始化，完整游玩与微软登录仍待验收 |
| Windows x64 | 已有构建、测试与部分安装记录；交互桌面、图形游戏和新账号后端仍待完整验收 |
| Linux x64 | 已有构建、测试与部分安装记录；图形游戏和新账号后端仍待完整验收 |
| 其他架构 / 旧系统 | 尚未验收；不能把 Qt 5 构建入口视为已支持旧 Windows 或 32 位系统 |

在「设置 → 通用与外观 → 界面语言」中选择语言，重启启动器后生效。
未知系统语言回退英文，繁体中文系统使用繁体；项目介绍、实例名称、版本标识和游戏日志保留原文。
翻译维护与验收方式见[国际化说明](docs/internationalization.md)。

## 构建与运行

需要 CMake 3.21+、C++17 编译器、zlib 和 Qt 6.5+：Core、Widgets、Network、Concurrent、Svg；
翻译构建需要 LinguistTools，测试另外需要 Qt Test。读取 WebP 需要部署 Qt Image Formats 的 WebP 插件。

将 `/path/to/Qt` 替换为本机 Qt SDK 的安装前缀：

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="/path/to/Qt" -DMCLAUNCHER_QT_MAJOR=6 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

常见输出路径：

- macOS：`build/m2cl.app/Contents/MacOS/m2cl`
- Linux：`build/m2cl`
- Windows：`build/Release/m2cl.exe`，或单配置构建的 `build/m2cl.exe`

源码中保留 Qt 5.15 构建选择器，但尚未完成该工具链的编译与兼容验证。
当前开发包也未完成独立部署、签名及安装器验收；仅复制可执行文件不能替代 Qt 插件部署。
构建、测试和部署说明见[构建指南](docs/build-and-validation.md)。

## 数据、来源与账号

- 设置默认写入系统应用配置目录；`--data-dir` 可使用独立目录。实例运行时和启动选项保存在各实例中。
- Java 下载接入 Temurin、Zulu、Microsoft OpenJDK 和 Oracle OpenJDK；游戏所需的官方运行时另作自动补齐来源。
- 资源市场当前接入 Modrinth。CurseForge 计划在完整首版后申请本项目 API Key；尚未接入。
- 微软登录使用本项目自己的公共 Client ID；API 访问许可及真实登录仍待完成验证。
  已保存的刷新凭据与普通设置分开，使用系统凭据库；不可用时可明确选择仅当前会话。
- 内置版本封面是 AI 生成的主题插画，不是游戏实拍，来源与打包说明见[封面资源](assets/covers/README.md)。

## 接下来的工作

- 准备符合官方要求的账号与游戏资格验证流程，申请 API 访问并完成真实登录 / 启动验收。
- 持续校对三种语言的术语、长文本，以及 Windows / Linux 字体布局。
- 扩展模组与光影兼容性验证、旧加载器支持，以及 Windows / Linux 图形验收。
- 整合包导入、下载暂停与持久恢复、独立发布包与性能测量。

## 贡献说明

- [构建与测试](docs/build-and-validation.md)
- [翻译维护](docs/internationalization.md)

## 许可证与署名

Copyright (C) 2026 **ppm203**。

项目原创源码采用 **GNU GPLv3（仅第 3 版）及第 7 节附加条款**：
[完整许可证](LICENSE)、[署名与修改版标识条款](ADDITIONAL_TERMS.md)、[版权及第三方声明](NOTICE)。
分发修改版时需保留合理署名、准确说明原始来源，并合理标明第三方修改版，避免冒充原作者发行或认可的版本。
附加条款不禁止 Fork、商业分发或加入广告，也不要求保留固定 Logo、链接或界面布局。

第三方组件继续使用各自的许可证，MD4C 的 MIT 声明保留在[原文件](third_party/md4c/LICENSE.md)。
M²CL 是独立项目，与 Mojang Studios、Microsoft 无官方隶属或合作关系。
