# Minecraft Injected Client Proxy

Minecraft 1.20.1 客户端反向代理的桌面管理工具（Aurora.dll 的注入器 / 控制台），**macOS 风格图形界面**。

**原生 Win32 + GDI+ 实现，无任何运行时依赖** —— 不需要 Qt、.NET 或 VC++ 运行库，构建产物为单个 exe（约数百 KB），冷启动即时。

## 它做什么

- **A 客户端**：运行 Minecraft 1.20.1 并进入目标服务器（本工具把代理 DLL 注入到这个进程）
- **B 客户端**：使用「直接连接」访问 `127.0.0.1:25565`，与 A 共享同一条会话
- 本程序负责：定位目标窗口 → 注入 `Aurora.dll` → 探测代理端口就绪 → 实时查看代理日志

## 功能总览

| 页面 | 内容 |
| --- | --- |
| 总览 | 代理运行状态、B 客户端连接数、一键注入、当前配置摘要、局域网连接提示 |
| 注入 | DLL 路径 / 窗口关键字 / 端口，分步进度（检查 DLL → 定位窗口 → 注入 → 等待监听）与结果横幅 |
| 日志 | 实时尾随 `proxy.log`（自动探测常见位置）、级别着色与过滤、自动滚动、跳转系统记事本 |
| 设置 | 主题（跟随系统 / 浅色 / 深色）、窗口材质（云母 / 亚克力，实验性）、注入与日志行为、恢复默认 |
| 关于 | 版本、仓库链接、以管理员身份重启 |

界面语言为简体中文；默认窗口关键字「布吉岛」、默认端口 25565，均可在「注入」页修改。

## 界面与工程

- macOS 风格：无边框自绘窗口、红黄绿窗口控制、毛玻璃侧边栏、胶囊选中态、分段控件、圆角卡片、Toast 浮窗
- DWM 集成：Win11 圆角 + 沉浸式深色 + 系统材质（Mica / Acrylic，通过 32bpp ARGB 双缓冲与 `DwmExtendFrameIntoClientArea` 实现，Win10 自动回退）
- Per-Monitor V2 高 DPI、跟随系统的深浅色切换（`WM_SETTINGCHANGE`）
- 注入引擎移植自上游 Compass（反射式注入，`src/core/injectcore/`），支持驻留代理恢复信号
- 端口探测基于 `GetExtendedTcpTable`（不建套接字），可判断"目标 PID 是否已监听端口"与 B 端连接数

## 从源码构建

要求：Windows + Visual Studio 2022（或仅 Build Tools）+ CMake 3.21+，x64。

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel
# 产物：build\bin\Release\MinecraftInjectedClientProxy.exe
```

也可在 MinGW-w64 工具链下构建（CMake 会自动识别）。

## 使用方法

1. 取得 `Aurora.dll`（代理核心，**不随 Release 附带**，见下节）
2. 将 DLL 与本程序放在同一目录（或在「注入」页手动选择文件）
3. 启动 A 客户端（Minecraft 1.20.1）并进入目标服务器
4. 点击「一键注入」；若游戏以管理员身份运行，请在「关于」页以管理员身份重启本程序
5. B 客户端「直接连接」`127.0.0.1:25565`（局域网内使用总览页显示的本机 IP）

注入成功后，程序会持续监视代理状态（进程存活 / 端口监听 / B 连接数）。

## 关于 Aurora.dll 的分发

本仓库只包含**图形界面与注入逻辑**；代理核心 `Aurora.dll` 的源码不在本仓库中，因此 Release 默认只附带 GUI。

如果希望随包分发，把 `Aurora.dll` 提交到**仓库根目录**即可 —— CI 会检测到它并自动打进发布包（见 `.github/workflows/ci.yml` / `release.yml` 的打包步骤）。未检测到时发布说明会提示用户自行放置。

## 持久化

设置保存在 `%APPDATA%\MinecraftInjectedClientProxy\settings.ini`（INI，UTF-16，含窗口位置）。日志自动探测顺序：`%APPDATA%\.minecraft\proxy.log` → `%TEMP%\MinecraftProxy.log` → `C:\MinecraftProxy.log`。

## 免责声明

仅供学习与技术研究用途。使用者需自行遵守 Minecraft EULA 及所玩服务器的规则；因滥用造成的后果与本项目无关。
