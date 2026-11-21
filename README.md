# Minecraft Injected Client Proxy — GUI

[![CI](https://github.com/ColumbinaHyposeleniaYS/MinecraftInjectedClientProxy/actions/workflows/ci.yml/badge.svg)](https://github.com/ColumbinaHyposeleniaYS/MinecraftInjectedClientProxy/actions/workflows/ci.yml) [![Release](https://img.shields.io/github/v/release/ColumbinaHyposeleniaYS/MinecraftInjectedClientProxy?label=release)](https://github.com/ColumbinaHyposeleniaYS/MinecraftInjectedClientProxy/releases/latest)

Minecraft Java **1.20.1** 注入式客户端反向代理的图形界面。A 客户端保持与远端服务器的真实连接；代理 DLL 注入到 A 进程后在本机开启 **25565** 监听，B 客户端连接该端口即可进入 A 所在的服务器，共享世界、物品栏与位置。

本程序将注入器完整移植进 GUI：等待目标窗口、反射式注入、恢复驻留代理、探测端口就绪，全部在 C++ 中完成，不依赖任何网页资源。

## 下载

- 在 [Releases](https://github.com/ColumbinaHyposeleniaYS/MinecraftInjectedClientProxy/releases/latest) 页面获取 Windows x64 便携版压缩包（已内嵌 Qt 运行库，解压即用，附 SHA-256 校验和）。
- 推送到 `main` 的每次构建产物也可从 [Actions](https://github.com/ColumbinaHyposeleniaYS/MinecraftInjectedClientProxy/actions/workflows/ci.yml) 页面下载。

## 界面

借鉴 macOS 应用视觉语言的 Windows 原生（Qt 6）桌面程序：

- 无边框窗口、自绘标题栏，左上角红黄绿圆点映射 **关闭 / 最小化 / 最大化**
- 通过 DWM（`nativeEvent` + `DwmSetWindowAttribute`）应用圆角窗口与可选 **Mica / 亚克力** 背景（Windows 11 22H2+，设置中开启）
- 220 px 毛玻璃侧边栏、12 px 圆角卡片、8 pt 网格、胶囊选中态、分段控件、开关
- 浅色 `#F5F5F7` / 深色双主题，强调色 `#0A84FF`，Segoe UI Variable 字体链
- 全部控件由 QPainter 自绘（含 1.5 px 线性图标），hover / focus / active / disabled / 空状态 / Toast 通知齐备
- 高 DPI 缩放、原生 Aero Snap、右键标题栏弹出系统菜单

## 功能页面

| 页面 | 内容 |
| --- | --- |
| 总览 | 代理运行状态、A 客户端、监听端口、B 连接数实时卡片；快速上手与注意事项 |
| 注入 | DLL 路径与窗口关键字配置、五步注入进度（等待窗口 → 匹配进程 → 注入 → 等端口 → 完成）、30 秒进度条、管理员权限横幅 |
| 日志 | 自动探测并实时跟随 `proxy.log`，按严重级别过滤着色，跟随滚动/清空/打开目录 |
| 设置 | 主题、窗口材质、注入默认值、日志来源覆盖、管理员重启、恢复默认 |
| 关于 | 产品标识、运行要求、使用要点 |

## 构建要求

- Windows 10 / 11 x64
- **Qt 6.5+**（Widgets 模块）
- **Visual Studio 2022**（含 C++ 桌面开发与 CMake 组件）或 MSVC Build Tools
- CMake 3.21+（VS 自带）

MSVC x64 是唯一受支持的工具链（注入核心为 64 位 PE 操作）。

## 构建步骤

在 **x64 开发者命令行 / Developer PowerShell** 中执行：

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=<你的 Qt 安装路径>
cmake --build build --config Release --parallel
```

产物为 `build\Release\MinecraftInjectedClientProxy.exe`（或 `build\` 下按生成器布局）。

如需使用 Ninja：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<Qt 路径>
cmake --build build --parallel
```

## 使用说明

1. 准备代理 DLL（`Aurora.dll`，由代理引擎构建产出），放在程序同目录即可自动探测，或在「注入」页手动选择。
2. 启动 A 客户端并进入目标服务器（也可先跳过，采用“入服前注入”）。
3. 在「注入」页点击 **开始注入**。程序会等待标题包含关键字（默认“布吉岛”，可在设置中修改）的 Java 窗口，完成反射注入并等待 25565 端口就绪。
4. B 客户端使用「直接连接」输入 `127.0.0.1:25565`（同机）或 `A 的局域网 IP:25565`。
5. A 窗口在前台时按 **DEL** 可随时停用代理；再次点击「开始注入」即可恢复（对驻留代理发送恢复事件，不会重复加载 DLL）。

> 若游戏以管理员身份运行，请通过「注入」页或「设置 → 高级」以管理员身份重启本程序。

## 设置文件

所有偏好保存在 `%APPDATA%\MinecraftInjectedClientProxy\settings.ini`，可通过「设置 → 高级 → 打开设置目录」直达，支持一键恢复默认。

## 项目结构

```
CMakeLists.txt
resources/            应用图标（由 scripts/gen_icon.py 生成）
src/
  main.cpp
  app/                界面层：主题令牌、图标库、基础控件、标题栏、侧边栏、主窗口
  pages/              总览 / 注入 / 日志 / 设置 / 关于
  core/               服务层：设置、路径、进程定位、端口探测、注入序列、状态监视、日志跟随
    injectcore/       注入核心（C）：反射加载、远程加载、驻留恢复信号
```

## 持续集成与发布

仓库内置两套 GitHub Actions 工作流（`.github/workflows/`）：

- **CI**（`ci.yml`）：推送到 `main` 或提交 PR 时，在 GitHub 托管的 Windows x64 环境中安装 Qt 6.8.3 LTS（MSVC 2022，带缓存）并完成 Release 构建、`windeployqt` 打包，产物可在工作流页面下载。
- **Release**（`release.yml`）：推送 `v*` 标签即自动构建并在 [Releases](https://github.com/ColumbinaHyposeleniaYS/MinecraftInjectedClientProxy/releases) 页发布便携版压缩包与校验和（含自动生成的中文更新日志；标签带 `-` 后缀时自动标记为预发布）：

```powershell
git tag v1.0.0
git push origin v1.0.0
```

  也可在 Actions 页面手动触发 Release 工作流并指定标签（标签不存在时会在当前提交上创建）。

## 已知限制

- 仅支持 Minecraft Java 1.20.1 与 Windows x64
- 反射注入按 64 位 PE 解析，DLL 与目标进程架构必须一致
- B 以离线身份加入，不进行 Mojang 正版验证
- 更换磁盘上的 DLL 后需重启游戏，新版本才会被加载

## 免责声明

仅供学习交流使用。请遵守所玩服务器的规则，合理使用本工具。
