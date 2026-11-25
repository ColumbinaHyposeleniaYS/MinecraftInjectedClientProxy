# 项目交接文档 · Minecraft Injected Client Proxy

> 版本 1.0.7 · 分支 main · HEAD 672cc40 · 远端 github.com/ColumbinaHyposeleniaYS/MinecraftInjectedClientProxy
> 本文档面向接手开发者：讲清楚它是什么、怎么构建、代码怎么组织、状态机怎么走、哪些地方容易踩坑。

---

## 0. 一句话说明

一套 Windows x64 桌面工具：图形界面（MinecraftProxy.exe）+ 可注入的代理核心（Proxy.dll）。把代理核心注入 Minecraft 1.20.1 / 1.21.10 NeoForge 客户端 A 的 Java 进程后，A 与真实服务器的连接变成一条可编程中继流；此时另一个客户端 B 通过 127.0.0.1:1314 接入，与 A 共享同一条服务器会话。

代理核心以 RCDATA 资源内嵌在 exe 里，运行时提取到 %TEMP% 再注入，因此分发包只有单个 exe。

---

## 1. 当前仓库状态

| 项 | 值 |
| --- | --- |
| 远端 | https://github.com/ColumbinaHyposeleniaYS/MinecraftInjectedClientProxy.git |
| 分支 / HEAD | main / 672cc405735ad1534058da82e7c0f6cf42c682eb |
| 最新 Release | v1.0.7 |
| 源码规模 | 约 13350 行（GUI 约 4700 行 / 代理核心约 8000 行 / 注入引擎约 600 行） |
| 构建方式 | 仅 GitHub Actions（windows-latest + MSVC x64）。本机未装 MSVC/CMake，本地编译从未验证 |
| CI | ci.yml push 即构建，产物为 zip artifact，保留 14 天 |
| Release | release.yml 由 v* tag 触发，自动生成 zip + checksums + 发布说明 |

**接手第一件事**：本地没有可用 MSVC 工具链，任何改动都要走 git push 到 GitHub Actions 验证。CI 日志里的 error C#### 是唯一可靠的编译反馈。

---

## 2. 构建与发布

### 2.1 本地构建（需自备工具链）

要求：Windows + Visual Studio 2022（或 Build Tools）+ CMake 3.21+，x64。

    cmake -S . -B build
    cmake --build build --config Release --parallel
    # 产物：
    #   build/bin/Release/MinecraftProxy.exe   <- GUI（内嵌 Proxy.dll）
    #   build/bin/Release/Proxy.dll            <- 代理核心（嵌入后仍保留在输出目录）

MinGW-w64 也能编 GUI，但会跳过 Proxy.dll（CMake 打印 "Proxy.dll (proxy core) needs the MSVC toolchain - building the GUI only."），此时 MICP_HAS_EMBEDDED_DLL 未定义，注入功能不可用。

### 2.2 关键 CMake 约定

- 静态 CRT（/MT）：DLL 注入的 JVM 进程没有 VC 运行时，exe 也要免安装。见 CMakeLists.txt:26-28。
- 代理核心源清单：proxy/sources.txt（21 行）是 aurora 目标的唯一源文件列表，由 file(STRINGS) 读取，并注册 CMAKE_CONFIGURE_DEPENDS 使修改后自动重配。新增 .cpp 必须登记，否则不参与编译。
- 版本号：project(... VERSION 1.0.7)。Release 构建用 -DMICP_APP_VERSION=<tag> 覆盖，编译进 MICP_APP_VERSION 宏供「关于」页显示。
- 构建种子：configure 时用 UTC 时间戳生成 MICP_BUILD_SEED，注入的类名/包名由它派生（见 5.6）。
- 内嵌资源链（CMakeLists.txt:122-137，顺序不能错）：
  1. file(WRITE build/proxy_embed.rc "AURORA_DLL RCDATA ""Proxy.dll""")
  2. add_custom_command 把 $<TARGET_FILE:aurora> 复制成 build/Proxy.dll
  3. add_custom_target(copy_aurora_dll) + add_dependencies(micp copy_aurora_dll) —— 保证 rc 编译时 DLL 已存在
  4. target_sources(micp PRIVATE proxy_embed.rc) + MICP_HAS_EMBEDDED_DLL

  改这里最常见的事故是：DLL 没先复制，rc 编译报找不到 Proxy.dll。

### 2.3 发版流程

    # 1) 改 CMakeLists.txt 的 VERSION
    # 2) 提交
    git commit -am "版本升至 x.y.z"
    # 3) 打 tag 并推送（触发 release.yml）
    git tag vx.y.z && git push origin main vx.y.z

release.yml 行为：校验 tag 格式 -> 构建 -> 打包 dist/MinecraftProxy-<tag>-win64.zip（只有 exe + README）+ checksums.sha256 -> 用 git log <prev>..HEAD 生成 changelog -> gh release create（已存在则 upload --clobber + edit）。
注意它设了 [Console]::OutputEncoding = UTF8，否则中文提交信息会乱码。

---

## 3. 目录结构

    CMakeLists.txt              构建入口（GUI + 代理核心 + 内嵌资源）
    resources/                  appicon.rc（manifest + 图标）、app.manifest、aurora.rc（历史遗留，实际嵌入 rc 由 CMake 生成）
    tools/gen_lang.py           12 语言字符串表生成器（唯一真源）
    scripts/gen_icon.py         图标生成辅助

    src/main.cpp                WinMain：无边框窗口、单实例、DPI、消息循环
    src/common.h                公共预置（winsock2 先于 windows.h 等）
    src/app/                    GUI
      App.cpp/.h                状态机、布局计算、输入路由（2238 行，最大单文件）
      Paint.cpp                 全部绘制（1028 行）
      Ui.cpp/.h                 基础控件：按钮/开关/分段/卡片/滚动条
      Theme.cpp/.h              调色板、字体、DWM 材质
      Lang.cpp/.h               语言切换与 Tr() 查表
      lang/                     [生成] StrTable.h + 12 个语言头（纯 ASCII \uXXXX 转义）
    src/core/                   注入与系统集成
      EmbeddedDll.cpp/.h        从自身 RCDATA 提取 Proxy.dll 到 %TEMP%
      InjectionSequence.cpp/.h  注入四步状态机（工作线程）
      Injector.cpp/.h           injectcore 引擎的薄封装
      injectcore/               反射式注入实现（ReflectiveLoader 移植）+ RuntimeControl.h 事件契约
      ProcessFinder.cpp/.h      按窗口标题关键字找 Java 进程
      PortProbe.cpp/.h          GetExtendedTcpTable 探测监听/连接数（不建套接字）
      ProxyMonitor.cpp/.h       周期健康快照（进程/监听/B 连接数）
      LogTail.cpp/.h            日志尾随
      Settings.cpp/.h           INI 持久化
      AppPaths.cpp/.h           已知路径、日志候选位置、局域网 IP

    proxy/                      代理核心（编译为 Proxy.dll）
      sources.txt               唯一源清单
      loader.cpp                DllMain -> RuntimeThread 主循环
      env.cpp                   LogTo / Dbg / ClassLoader 查找 / 调试日志开关
      connection_hook.cpp       JVMTI ClassFileLoadHook（改 Connection.channelActive）
      class_edit.cpp/.h         字节码插入与栈映射表重算
      classfile.cpp/.h          ClassBuilder（运行时生成 class）
      trampolines.cpp           HookBridge 类定义与 native 绑定
      relay_handler.cpp         RelayHandler（Netty pipeline 中继）+ bypass 队列
      packet_filter.cpp/.h      「虚空模式」：A 端渲染包过滤（类名缓存）
      packet_policy.h           A/B 双向发包允许规则
      chat_notify.cpp/.h        A 端聊天提示（12 语言）
      random_name.cpp/.h        构建种子派生的随机类名/包名
      runtime_gate.h            运行时回调准入闸（stop 时等待 in-flight 归零）
      login_handoff.h           快照握手状态机（20s 超时）
      world_snapshot.cpp        主线程抓世界快照
      status_response.cpp       Server List Ping MOTD
      jni_lookup.cpp            JNI 反射辅助、JniClassName
      jni_inject.cpp            [不参与构建] 旧 companion injector，仅供参照
      server/                   代理服务端
        state.h/.cpp            全局 ServerState 单例
        bindings.h/.cpp         全部 JNI 引用与方法 ID 缓存（约 120 字段）
        transport.cpp           bind / LAN 公告 / 协议状态切换
        lifecycle.cpp           启停、连接捕获、主线程闸
        login.cpp               登录握手、快照重建
        forwarding.cpp          B<->A 数据转发、bundle 展开
        teams.cpp               队伍包处理
        internal.h              内部接口总表

---

## 4. GUI 层（src/app）

### 4.1 结构

App 是唯一的状态持有者：设置、注入序列、代理监视器、日志缓冲、页面/布局、输入路由。main.cpp 的 WndProc 只做消息分发。

无边框窗口：WS_POPUP | WS_THICKFRAME（故意没有 WS_CAPTION，这样 DWM 永远不会在自绘界面上画出原生标题栏）。标题栏按钮、调整大小、拖拽全部自己实现（WM_NCHITTEST -> App::HitTestChrome）。

单实例通过命名互斥体 Local\MinecraftInjectedClientProxy.SingleInstance 实现；「以管理员运行」用 --micp-takeover <pid> 完成无缝提权接管（main.cpp:267-320）。

### 4.2 五个页面

| PageId | 内容 |
| --- | --- |
| PgOverview | 运行状态、一键注入、当前配置摘要、B 连接提示（本机 + 局域网 IP） |
| PgInject | 关键字/端口输入、四步进度、结果横幅、断开服务器按钮 |
| PgLogs | 日志尾随、级别过滤、自动滚动、清空/打开文件 |
| PgSettings | 外观（主题/材质/语言）、行为（5 个开关）、日志来源、恢复默认 |
| PgAbout | 版本、仓库链接、以管理员身份重启 |

### 4.3 控件 ID（App.h）

- ID_TL_CLOSE=1 / MIN=2 / MAX=3 窗口控制
- ID_NAV=10 + pageIndex 侧边导航
- ID_BTN_INJECT=20 ... ID_BTN_BROWSE_LOG=32 按钮
- ID_TGL_FOLLOW=40 / AUTOSWITCH=41 / TOASTDONE=42 / DEBUG=43 / CLEARLOG=44 开关
- ID_SEG_THEME=50 / BACKDROP=51 / LOGFILTER=52 分段控件
- ID_ROW_LOG=61 可点击行、ID_LANG_ROW=70 / ID_LANG_ITEM=71+i 语言选择
- ID_PAGE_SB=90 / ID_LOG_SB=91 滚动条命中区
- ID_EDIT_KEYWORD=100 / ID_EDIT_PORT=101 原生 EDIT 子窗口

加控件要改三处：App.h 的枚举与矩形数组 -> App.cpp 的 Layout 计算 + 点击处理 -> Paint.cpp 的绘制。漏掉任一处通常表现为「画出来了但点不动」或「点了没反应」。

### 4.4 布局与缩放

所有尺寸走 S(x)（DPI 缩放宏）；字体走 FBody/FHead/FSmall。页面内容高于视口时自动出现滚动条（pageContentH 控制）。布局在 m_layoutDirty 时重算并缓存到 Layout 结构。
---

## 5. 代理核心（proxy -> Proxy.dll）

### 5.1 生命周期总览

    DllMain(DLL_PROCESS_ATTACH)
      -> CreateThread(RuntimeThread)

    RuntimeThread（proxy/loader.cpp）
      - 等 jvm.dll 加载（最多 60s，100ms 轮询）
      - JNI_GetCreatedJavaVMs -> AttachCurrentThreadAsDaemon("nioEventLoopGroup-3-1")（普通线程名，避免被反作弊按 thread_list 扫语义关键词）
      - GetEnv(JVMTI_VERSION_1_2)
      - 主循环（每轮 25ms 忙 / 250ms 闲）
          - active   -> BServer_CheckLoginTimeout()
          - !active && !clean -> 每 1s 重试 stopSession()
          - requested && !active && clean -> startSession()
          - WaitForMultipleObjects({resume, disconnect}, active?25:250)

两个命名事件是 GUI 与 DLL 的通信通道（契约头：src/core/injectcore/RuntimeControl.h）：

| 事件 | 前缀 | 作用 |
| --- | --- | --- |
| 恢复 | MICP_RESUME_PREFIX<pid> | GUI 注入时 SetEvent 唤醒驻留 DLL 重新开始会话；DLL 侧若 ERROR_ALREADY_EXISTS 说明已有实例，置位后自己退出 |
| 断开 | MICP_DISCONNECT_PREFIX<pid> | GUI「断开服务器」按钮 -> DLL 关闭 A 到真实服务器的连接（代理仍驻留） |

### 5.2 startSession / stopSession

startSession（loader.cpp:23-56）：
1. 等 MC classloader 就绪（最多 240 次 × 100ms，期间响应 DEL），InstallRelayHandler
2. g_runtimeGate.start()（从此刻起放行 JNI 回调）
3. InstallBServer（bind 配置端口，见 §6.1 / PortConfig.h）
4. 最多 10 次 BServer_TryCaptureLiveConnection —— 从 Minecraft.getConnection().getConnection().channel.pipeline() 直接拿 A 的真实连接并挂 Relay
5. A 已连服 -> Captured；未连服 -> NotConnected（旧版会走 BServer_BlockAMainThreadUntilBConnected，见下方"已废弃"）

stopSession（loader.cpp:59 起）：先 BServer_RequestStop() 放掉主线程闸 -> g_runtimeGate.stopFor(1000ms) 等回调归零 -> StopBServer + RelayHandler_DetachAll。失败返回 false，主循环 1 秒后重试（日志里连续出现的 "STOP: draining native callbacks" 就是这个重试循环，已做节流）。

### 5.3 注入钩子链（纯 mid-session，无字节码改写）

    BServer_TryCaptureLiveConnection（JNI，仅 mid-session）
      -> Minecraft.getInstance().getConnection().getConnection().channel.pipeline()
           -> RelayHandler_AttachToPipelineObject(pipeline)
                -> pipeline.addBefore("packet_handler", 随机 handler 名, new RelayHandler())
           -> BServer_SetTargetConnection(A 的 Connection)

**关键设计（Anti-Cheat 规避）**：代理核心不再改写任何 Minecraft 类字节码。
- 已彻底移除 JVMTI ClassFileLoadHook / Retransform / ClassFileEditor 整条链路：proxy/connection_hook.cpp、proxy/trampolines.cpp、proxy/class_edit.cpp 及其头文件已从 proxy/sources.txt 删除，不再编译进 DLL。
- 动机：反作弊 native 层校验 InstanceKlass/_methods/ConstantPool 完整性（"Method code must not be zero"），旧版往 Connection.channelActive 头部插 invokestatic HookBridge.onChannelActive 会命中该检测面。现在 Java 侧没有任何被改写的 MC 类。
- 代价：**"进服前注入"路径随之废弃**。旧版靠 channelActive hook 在 A 首次连服时自动捕获；现在必须在 A 已进服后注入（网易版启动即进服，正好符合）。BServer_BlockAMainThreadUntilBConnected / defineMainGateClass 仍留在源码但 mid-session 时不会调用，实际不可用。
- 动态类白名单规避：handler 名用 random_name 生成（无固定前缀，躲 pipeline().names() 枚举）；线程 attach 名改成 nioEventLoopGroup-* 普通名（躲 JavaThread/_thread_list 语义扫描）；命名事件前缀改成不透明 GUID（躲 OpenEvent 枚举）。剩余运行时定义的类（RelayHandler / ServerInit / snapshot Thread）仍是"陌生类"，但数量已压到最少。

### 5.4 中继（RelayHandler）

proxy/relay_handler.cpp，一个 ChannelDuplexHandler 子类，两个 native 方法：

入站 channelRead（服务器 -> A）

    持 BServer_DispatchMutex
    - packet_filter::HideFromA(packet) 命中 -> 只 BServer_ForwardToB()，不 fireChannelRead（A 不渲染）
    - 否则 -> fireChannelRead() + BServer_ForwardToB()

出站 write（A -> 服务器）

    bypass = consumeBypassMark(packet) || packet_policy::allowFromA(cls, BServer_IsBActive())
    - 允许 -> ctx.write(packet, promise)
    - 拒绝 -> promise.setSuccess()（静默丢弃）

bypass 机制：代理自己往 A 通道写的包（快照、聊天提示）会先 RelayFilter_MarkBypass 记入全局队列，这样它们不会被 allowFromA 误杀。队列上限 256，超过删头。

### 5.5 包策略与「虚空模式」

packet_policy.h：
- gamePrefix = "net.minecraft.network.protocol.game."
- A 独占：ServerboundKeepAlivePacket、ServerboundCustomPayloadPacket
- allowFromA(name, bActive) = !bActive || !isGame || ownedByA —— B 不在线时 A 完全自由；B 在线时 A 只保留心跳与 mod 握手
- allowFromB(name) = isGame && !ownedByA

packet_filter.cpp（22 类渲染包）：区块（LevelChunkWithLight / LevelChunk / ForgetLevelChunk / LightUpdate）、聊天（SystemChat / PlayerChat / PlayerChatHeader / DisguisedChat）、标题（SetActionBarText / SetTitleText / SetSubtitleText / SetTitleTextAnimation / ClearTitles）、记分板与队伍（SetObjective / SetDisplayObjective / SetScore / SetPlayerTeam）、BossEvent、音效（Sound / SoundEntity / StopSound / LevelEvent）。

性能关键点：类名 -> 是否隐藏的结果缓存在 unordered_map<string,bool>（HiddenByName），命中路径零分配；未命中才走一次 JVMTI GetClassSignature。后缀比较用 memcmp 而非构造 std::string。改动这里要保持零分配性质，否则进世界时（区块瞬时倾泻几百包）会明显卡顿。

bundleFullyHidden：ClientboundBundlePacket 只有在每个子包都是渲染包时才整体丢弃；混合 bundle 必须整体放行，否则 A 的协议状态会乱。

### 5.6 运行时类生成与随机名

代理需要在运行时现场生成若干 Java 类（HookBridge、RelayHandler、ServerInit、ServerHandler、MainGate、Snapshot 桥）。classfile.cpp 的 ClassBuilder 拼字节码，env->DefineClass 在 Minecraft 的 ClassLoader 里定义。

名字由 random_name.cpp 生成：FNV-1a 的 MICP_BUILD_SEED ^ pid ^ 调用计数器，字符集为辅音/元音交替。

警告：这里踩过坑。最初每次调用都用同一个种子新建 mt19937_64，导致同进程内所有调用产出同一个名字，第二个类 DefineClass 直接抛 LinkageError: attempted duplicate class definition。修法是加 static std::atomic<uint64_t> s_callCounter。任何改动随机名逻辑的提交，都要保证「同进程内多次调用名字不同」。

### 5.7 服务端会话（proxy/server）

    B 连接建立
      -> initChannel：TCP_NODELAY、configureSerialization、addLast("bside")
           -> handleClientPacket
                - IntentPacket -> 切 STATUS 或 LOGIN（同时校验是否已占用）
                - StatusRequest -> BuildStatusResponse（MOTD，LAN 公告用）
                - PingRequest -> Pong
                - LoginHello（AwaitLogin 状态）-> completeLogin
                     - OfflinePlayer:<name> -> UUID -> GameProfile
                     - ClientboundGameProfilePacket 发给 B
                     - setProtocolState(PLAY)
                     - midSession ? reconstructAndSendLoginToB（抓世界快照）
                     - sendSelfInfoToB（B 自己的 player info）
                     - 进入 Play -> ChatNotify_NotifyProxyReady（A 端聊天提示）
                     - 放行主线程闸，A 开始连服，实时流喂给 B

状态机（state.h）：AwaitHandshake -> AwaitLogin -> Play；mid-session 路径额外经过 Syncing -> AwaitReady。

锁序（必须遵守）：dispatchMutex -> 然后才是 clientMutex 或 targetMutex。绝不能在持锁时等待 Minecraft 主线程（会死锁）。

世界快照（world_snapshot.cpp）：mid-session 场景下，B 后连时要把 A 当前世界状态整体重建给 B。实现是 minecraft.execute 把任务排到 Render thread，在 beginSnapshot 边界之后一次性序列化区块/实体/玩家信息等十余种包，15 秒超时（LoginHandoff 另有 20s 握手超时）。实测 213 区块 / 424 包约 206ms。

主线程闸（lifecycle.cpp:288 BlockAMainThreadUntilBConnected）：仅在 A 尚未连服时使用——注入后先把 A 的 Render thread 闸住，等 B 连上再放行，避免 A 抢先连服导致快照逻辑复杂化。B 已连接时不会触发。

---

## 6. 设置、日志与持久化

### 6.1 settings.ini

位置：%APPDATA%\MinecraftInjectedClientProxy\settings.ini（INI，UTF-16）

| 节 | 键 | 说明 |
| --- | --- | --- |
| appearance | themePref / backdrop / lang | 主题 0/1/2、材质 0/1/2、语言索引（-1=跟随系统） |
| injection | windowKeyword / port / autoSwitchOnDone / toastOnDone | 默认关键字「布吉岛」、端口 1314 |
| logs | pathOverride / follow / debug / clearOnStart | debug 与 clearOnStart 由 DLL 侧读取 |
| window | x/y/w/h/maximized | 窗口位置 |

重要：代理 DLL 与 GUI 共享同一个 ini。DLL 在 proxy/env.cpp 的 LoadDebugLogConfig() 里读 [logs] debug 与 clearOnStart——这是目前唯一的 GUI->DLL 配置通道，没有额外 IPC。加新配置项如果 DLL 也要用，就照这个模式加。

### 6.2 日志

proxy/env.cpp:47 LogTo：
- 候选路径（按序尝试）：%APPDATA%\.minecraft\proxy.log -> %TEMP%\MinecraftProxy.log -> C:\MinecraftProxy.log
- 默认不写（[logs] debug=0）。开启后才写。
- 32MB 自动轮转（<name>.old，旧的 .old 被替换）
- debug=1 且 clearOnStart=1 -> 首次写入前删除三个候选旧日志
- Dbg() 是调试弹窗（MessageBox），即使日志关闭也会弹

历史教训：日志曾无轮转增长到 1.5GB，且 stopSession 重试循环每秒写 3 行。现在两者都治了（轮转 + 重试日志节流），不要再引入无节流的高频日志。

### 6.3 恢复信号与旧版兼容

GUI 侧 Injector 同时兼容新前缀 MinecraftInjectedClientProxy.Resume.v1. 与旧版 SakuraTools 前缀（老版本 DLL 仍在内存中驻留时不会重复注入，而是唤醒）。

---

## 7. 多语言（12 种）

- 唯一真源：tools/gen_lang.py。KEYS 列表定义 (enum 名, zh_CN 文本)，顺序决定 Str 枚举值；T 字典为 11 种语言提供翻译；zh_CN 表由 KEYS 自动生成。
- 生成：python tools/gen_lang.py -> src/app/lang/StrTable.h + 12 个语言头。输出纯 ASCII（非 ASCII 转 \uXXXX），所以 MSVC 不需要 codepage 设置。
- 当前：12 tables x 127 strings = 1524。
- 加字符串的完整流程：KEYS 加一行 -> 11 个语言表各加一行 -> 重跑生成器。生成器会校验缺失并报 "<lang> is missing translations for: ..."。
- 常见坑：nl_BE 与 nl_NL 有若干文案完全相同，用 str.replace 批量插入时只会命中第一个，必须按表内偏移分别处理。
- 运行时：lang::Tr(Str)，越界回退 zh_CN 再回退 L""。DetectSystemLang() 在 lang=-1 时按系统区域解析。

A 端的聊天提示（注入成功/暂停）有独立的 12 语言表，在 proxy/chat_notify.cpp 内嵌，语言取自 Minecraft 的 options.language，取不到退英文。

---

## 8. 已知问题与排查

### 8.1 【已修复】注入后 30 秒内未开始监听

现象：GUI 四步走完前三步，第四步「等待代理监听」超时。

已修复（端口传递 + 占用提醒）：
- GUI 注入前把配置端口写进 `%APPDATA%\MinecraftInjectedClientProxy\settings.ini` 的 `[inject] port`；DLL 侧 `proxy/PortConfig.h::ReadConfiguredPort()` 读同一个 ini，`proxy/server/state.h::ProxyPort()` 进程级缓存，DLL bind 的就是 GUI 等待的端口。旧的 kProxyPort 硬编码已删除。
- GUI 注入前用 `PortProbe::IsPortBusyByOther(config.port, target.pid)` 预检：端口被别的进程占用则直接 Failed + SeqPortBusy 文案（「端口 %u 已被其他程序占用，请在设置中修改后重试」），不再先闪「注入中」；等待超时也复查占用，占用时给同样提示。
- DLL bind 失败写明确日志 "FAILED to bind port %d - it is likely already in use ..."。
- 默认端口已从 25565 改为 1314（AppPaths.h / Settings.h / ProxyMonitor.h / InjectionSequence.h / PortConfig.h 默认值一致）。

仍可能的失败方向：
1. DLL 加载了但 InstallBServer 失败 —— cacheJavaRefs 某个 JNI 查找返回空
2. DLL 提取到 %TEMP% 失败（杀软拦截）

排查步骤：设置页开「记录代理调试日志」+「启动时清除旧日志」-> 重新注入 -> 读 %APPDATA%\.minecraft\proxy.log，重点看 START: / BServer: / bindServer 相关行。

### 8.2 A 端进入世界卡顿（已优化，待真机验证）

前提是 B 已连上、A 才进世界（B 未连时主线程闸会挡住 A，那是另一条路径）。
已做的优化：packet_filter 的类名缓存 + 零分配后缀比较（见 5.5）。提交 672cc40。尚未在真实环境回归验证。

### 8.3 编译错误排查清单

| 错误 | 原因 |
| --- | --- |
| C1083: Cannot open include file | 相对 include 路径写错；proxy/server 里的文件引用同级头用 "internal.h"，从 proxy/ 引用要写 "server/internal.h" |
| C2039: 'X': is not a member of 'proxy_server' | b_server.h 里的函数声明在全局作用域，实现在 lifecycle.cpp 的 using namespace proxy_server; 之后（即全局）。不要写进 namespace 块里，调用点也不要加 proxy_server:: 限定 |
| LNK2019 | 新文件忘了登记 proxy/sources.txt |

### 8.4 网络/工具链环境问题（本机特有）

- 访问 github.com 间歇性 reset / timeout，push 失败重试即可
- GitHub API 偶发 502，gh run list 重试可恢复
- run_code 里 process.env 为空：APPDATA/TEMP 需手工拼 C:\Users\<user>\AppData\...
- gh CLI 需 USERPROFILE / APPDATA / HOME 指向用户目录才能读到登录凭据
- gh run list 的 sha 列是短格式，用 jq select(.headSha=="<短sha>") 匹配不到；gh release view 的 --json 不支持 isLatest

---

## 9. 最近变更（便于理解上下文）

| 提交 | 内容 |
| --- | --- |
| 672cc40 | 调试日志双开关（GUI + DLL 读 ini）；进世界卡顿热路径优化（类名缓存） |
| 38aea60 | 版本升至 1.0.7 |
| 887069d | 修复随机类名重名导致的 LinkageError |
| c607036 / c805642 | 修复 BServer_DisconnectARemote 的作用域/限定符编译错误 |
| b682fd2 | 随机类名/包名改为按构建种子派生，移除词表 |
| e433da7 | 新增「断开服务器」功能（GUI 按钮 + DLL 事件） |
| 9c4c4f5 | 代理核心更名 Aurora.dll -> Proxy.dll；CI/Release 不再单独发 DLL |

---

## 10. 接手建议

1. 先跑通 CI：随便改一行注释 push，确认 Actions 绿，再动真代码。
2. 读代码顺序建议：README.md -> 本文档 -> proxy/loader.cpp（主循环）-> proxy/server/forwarding.cpp（数据流）-> proxy/relay_handler.cpp（中继）-> src/core/InjectionSequence.cpp（注入流程）。
3. 改 GUI 前先理解 Layout 的重算时机（m_layoutDirty）和 S()/FBody 缩放宏。
4. 改代理核心前先理解三把锁的关系和 RuntimeGate 的准入语义——这是最容易出死锁和 use-after-free 的地方。
5. 任何改动都要想清楚「A 在连服中 / A 未连服 / B 已连 / B 未连」四种状态组合。

---

## 10.1 1.21.10 NeoForge 适配现状（续作记录）

### 版本探测
- `proxy/server/version_detect.h/.cpp`（新增，已登记 `proxy/sources.txt`）：`DetectMcVersion` 先读 SharedConstants 的版本字符串（`1.21*` / `1.20*`），失败则对 `Connection.configureSerialization` 做结构探针（4 参带 `BandwidthDebugMonitor` = 1.21.10，2 参 = 1.20.1）。结果写入 `server.refs.version`，启动时打印 `BServer: detected Minecraft ...`。

### 版本相关 JNI 适配点
| 位置 | 1.21.10 差异 | 处理 |
| --- | --- | --- |
| `bindings.cpp` `cacheProtocolBindings` | `configureSerialization` 变 4 参 | `connectionConfigureModern` 开关：4 参 (pipeline, flow, false, null) / 2 参 |
| `bindings.cpp` 友 buf 加载 | 1.21 包需 `RegistryFriendlyByteBuf` | 加载该类 + `(ByteBuf,RegistryAccess)` 构造 + `ClientPacketListener.registryAccess()`，缓存到 `refs.registryBuf*`/`cplRegistryAccessMid` |
| `login.cpp` `sendSelfInfoToB` | PlayerInfoUpdate 用 registry-aware buf | 优先 new `RegistryFriendlyByteBuf(bb, registryAccess)`，缺失回退 `FriendlyByteBuf` |
| `world_snapshot.cpp` `captureNow` | LoginPacket / CommonPlayerSpawnInfo 记录字段与构造签名变化 | modern 分支用 try/catch 探测两种 spawn ctor（ResourceKey/Holder 顺序）与两种 login ctor arity；任一失败只跳过 login 包并打日志，不影响其余快照；1.20.1 走原构造 |
| `status_response.cpp` | ServerStatus 的 `isModded` 由 Optional 变 boolean | `StatusCtor::Modern` 探测 `(...,ZZ)V`，按 kind 选择 NewObject 入参 |
| `bindings.cpp` + `packet_policy.h` | CustomPayload 从 `protocol.game` 迁到 `protocol.common`（1.20.2+） | `customPayloadPacketCls` 改为 common 优先、game 回退双试加载；`isOwnedByA` 补 common 版 KeepAlive/CustomPayload 条目。行为本就由 `!isGamePacket` 保证 A 独占协议，此改动让 1.21.10 也能命中专用跳过分支与诊断日志 |

### 关键修复（本次续作）
- 恢复三个曾被整段截断的文件：`world_snapshot.cpp`、`server/login.cpp`、`server/transport.cpp`，以 HEAD 完整版为底重新合并上述增量，找回 `InstallWorldSnapshot`/`CaptureWorldSnapshot`/`WriteSnapshotPacket`/`completeLogin`/`uuidToBytes`/`defineInitClass`/`bindServer` 等函数定义（否则 LNK2019）。
- `world_snapshot.cpp` 补 `#include "jni_lookup.h"` 与 `#include "server/state.h"`，并把裸 `server.refs.version` 改为 `proxy_server::server.refs.version`（该文件在匿名命名空间内，原写法 C2065）。
- CustomPayload 包迁移：`bindings.cpp:436` 原先只加载 `game.ClientboundCustomPayloadPacket`，1.21.10 上找不到导致 `customPayloadPacketCls=null`，专用跳过分支与日志失效。改为 common 优先/game 回退双试；`packet_policy.h::isOwnedByA` 补 common 版两个包名。第三方/玩家自定义协议与 mod 协议走同一路径，A↔服务端照常、B 侧隔离的语义不变。

### 验证状态
- 已做：三文件括号/圆括号/方括号平衡校验通过；`internal.h` 声明的关键函数均有定义；新增字段与调用点一致；`version_detect.cpp` 已进源清单。
- 待做：本机无 MSVC/CMake，未做真实编译。须 git push 触发 GitHub Actions，确认无 C####/LNK2019 后再合入。
- 运行期风险：`world_snapshot` 的 CommonPlayerSpawnInfo 构造探测若全部未命中，B 会缺少 login 包（日志含 `skipping login packet`），其余快照与实时流不受影响；真机需核对 1.21.10 的记录 ctor 精确签名以固化探测表。

---

## 11. 免责声明

本项目仅供学习与技术研究。使用者需自行遵守 Minecraft EULA 及所玩服务器的规则；因滥用造成的后果与作者无关。
