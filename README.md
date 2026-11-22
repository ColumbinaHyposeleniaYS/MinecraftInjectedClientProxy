# Minecraft Injected Client Proxy

Minecraft 1.20.1 客户端反向代理的完整桌面套件：**macOS 风格图形界面**（注入器 / 控制台）+ **Aurora.dll 代理核心**（源码随仓库分发，Release 开箱即用）。

**GUI 为原生 Win32 + GDI+ 实现，无任何运行时依赖** —— 不需要 Qt、.NET 或 VC++ 运行库；exe 与 DLL 均静态链接 CRT，解压即用。

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

- macOS 风格：无边框自绘窗口、红黄绿窗口控制（已去除 Windows 原生标题按钮）、毛玻璃侧边栏、胶囊选中态、分段控件、圆角卡片、Toast 浮窗
- 页面内容超出窗口高度时自动出现滚动条（滚轮 / 拖动滑块均可）；输入框为原生 EDIT 控件，支持中文输入法、复制粘贴与全选
- DWM 集成：Win11 圆角 + 沉浸式深色 + 系统材质（Mica / Acrylic，通过 32bpp ARGB 双缓冲与 `DwmExtendFrameIntoClientArea` 实现，Win10 自动回退）
- Per-Monitor V2 高 DPI、跟随系统的深浅色切换（`WM_SETTINGCHANGE`）
- 多语言界面（12 种）：de_DE / en_US / ja_JP / nl_BE / nl_NL / pt_BR / pt_PT / ru_RU / tr_TR / uk_UA / zh_CN / zh_TW，首次启动跟随系统语言，设置页可随时切换（语言菜单以各自语言的原名显示）；字符串表由 `tools/gen_lang.py` 生成（`src/app/lang/`，纯 ASCII `\uXXXX` 转义，无编码依赖）
- 注入引擎移植自上游 Compass（反射式注入，`src/core/injectcore/`），支持驻留代理恢复信号
- 端口探测基于 `GetExtendedTcpTable`（不建套接字），可判断"目标 PID 是否已监听端口"与 B 端连接数
- 单实例运行：重复启动不会开第二个窗口，只会把已打开的窗口还原并置顶；「以管理员运行」由新实例接管（旧实例在提权成功后自动退出），UAC 取消则留在原地

## 目录结构

| 路径 | 内容 |
| --- | --- |
| `src/app` | GUI（窗口、绘制、主题、页面） |
| `src/core` | 注入引擎、窗口定位、端口探测、日志尾随、设置持久化 |
| `src/core/injectcore` | 反射式注入实现（与代理核心共享的恢复信号契约也在这里） |
| `proxy/` | **Aurora.dll 代理核心源码**（注入到 A 客户端的驻留代理本体） |
| `resources/` | 图标、清单 |

`proxy/sources.txt` 是代理 DLL 的唯一源码清单，新增源文件需在其中登记。

## 从源码构建

要求：Windows + Visual Studio 2022（或仅 Build Tools）+ CMake 3.21+，x64。

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel
# 产物：build\bin\Release\MinecraftProxy.exe
#      build\bin\Release\Aurora.dll（代理核心，MSVC 工具链时自动构建）
```

GUI 也可在 MinGW-w64 工具链下构建（此时跳过 Aurora.dll，仅生成 exe）。

## 使用方法

1. 下载 Release 压缩包并解压到任意目录（包内已含 `Aurora.dll`，无需额外获取）
2. 启动 A 客户端（Minecraft 1.20.1）并进入目标服务器
3. 点击「一键注入」；若游戏以管理员身份运行，请在「关于」页以管理员身份重启本程序
4. B 客户端「直接连接」`127.0.0.1:25565`（局域网内使用总览页显示的本机 IP）

注入成功后，程序会持续监视代理状态（进程存活 / 端口监听 / B 连接数）。游戏内按 DEL 可随时停用代理，再次注入即可恢复。

## Aurora.dll（代理核心）

代理核心源码位于 `proxy/`，随仓库一起构建，Release 自动打包，无需另外获取。

- 仅供注入到 Minecraft 1.20.1 客户端（x64 Java 进程），反射式加载、驻留游戏进程
- 恢复信号契约：DLL 在目标进程内创建 `MinecraftInjectedClientProxy.Resume.v1.<pid>` 命名事件；GUI 侧同时兼容旧版 `SakuraTools` 前缀
- 如需替换为自编译版本：把构建好的 `Aurora.dll` 放到仓库根目录，CI 打包时会优先采用根目录副本

## 持久化

设置保存在 `%APPDATA%\MinecraftInjectedClientProxy\settings.ini`（INI，UTF-16，含窗口位置）。日志自动探测顺序：`%APPDATA%\.minecraft\proxy.log` → `%TEMP%\MinecraftProxy.log` → `C:\MinecraftProxy.log`。

## 免责声明

仅供学习与技术研究用途。使用者需自行遵守 Minecraft EULA 及所玩服务器的规则；因滥用造成的后果与本项目无关。
