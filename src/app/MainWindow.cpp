#include "app/MainWindow.h"

#include "app/Page.h"
#include "app/Theme.h"
#include "app/TitleBar.h"

#include "core/AppPaths.h"
#include "core/Injector.h"
#include "core/LogTail.h"
#include "core/Settings.h"

#include "pages/AboutPage.h"
#include "pages/InjectPage.h"
#include "pages/LogsPage.h"
#include "pages/OverviewPage.h"
#include "pages/SettingsPage.h"

#include <QAbstractAnimation>
#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScreen>
#include <QTimer>
#include <QUrl>

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <dwmapi.h>

// DWM attributes (avoid depending on a specific Windows SDK).
namespace {
constexpr DWORD kDwmaImmersiveDarkMode = 20;
constexpr DWORD kDwmaWindowCornerPreference = 33;
constexpr DWORD kDwcpRound = 2;  // DWMWCP_ROUND
constexpr DWORD kDwmaSystemBackdropType = 38;
constexpr DWORD kDwmsbtNone = 1;
constexpr DWORD kDwmsbtMica = 2;     // DWMSBT_MAINWINDOW
constexpr DWORD kDwmsbtAcrylic = 3; // DWMSBT_TRANSIENTWINDOW
constexpr int kResizeBorder = 8;
} // namespace

MainWindow::MainWindow()
    : QWidget(nullptr)
{
    setWindowTitle(QString::fromUtf8(Theme::AppName));
    setMinimumSize(920, 640);

    // Sync persisted theme/backdrop into the Theme singleton before the
    // native window is created (translucency must be set first).
    Settings* settings = Settings::instance();
    Theme::setPref([settings]() {
        switch (settings->themePref())
        {
        case Settings::ThemePref::Light:
            return Theme::Pref::Light;
        case Settings::ThemePref::Dark:
            return Theme::Pref::Dark;
        default:
            return Theme::Pref::System;
        }
    }());
    Theme::setBackdrop([settings]() {
        switch (settings->backdrop())
        {
        case Settings::Backdrop::Mica:
            return Theme::Backdrop::Mica;
        case Settings::Backdrop::Acrylic:
            return Theme::Backdrop::Acrylic;
        default:
            return Theme::Backdrop::None;
        }
    }());

    if (Theme::translucentWindow())
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_NoSystemBackground);
    }

    buildUi();
    connectControllers();

    // Restore window placement.
    const QByteArray geometry = settings->windowGeometry();
    if (!geometry.isEmpty() && restoreGeometry(geometry))
        m_geometryRestored = true;

    applyGlobalTheme();
    setupNativeWindow();

    if (!m_geometryRestored)
    {
        const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
        const QSize size = QSize(1080, 720);
        resize(size.boundedTo(available.size() - QSize(32, 32)));
        move(available.center() - QRect(QPoint(0, 0), this->size()).center());
    }
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------------------

void MainWindow::buildUi()
{
    m_band = new TitleBand(this);
    m_band->setTitleText(QString::fromUtf8(Theme::AppName));

    m_sidebar = new Sidebar(this);
    m_sidebar->addItem(QStringLiteral("overview"), Icons::Icon::Gauge, QStringLiteral("总览"));
    m_sidebar->addItem(QStringLiteral("inject"), Icons::Icon::Inject, QStringLiteral("注入"));
    m_sidebar->addItem(QStringLiteral("logs"), Icons::Icon::Document, QStringLiteral("日志"));
    m_sidebar->addItem(QStringLiteral("settings"), Icons::Icon::Gear, QStringLiteral("设置"));
    m_sidebar->addItem(QStringLiteral("about"), Icons::Icon::Info, QStringLiteral("关于"));
    m_sidebar->setCurrentId(QStringLiteral("overview"));

    m_stack = new QStackedWidget(this);
    m_overview = new OverviewPage(this);
    m_injectPage = new InjectPage(this);
    m_logsPage = new LogsPage(this);
    m_settingsPage = new SettingsPage(this);
    m_aboutPage = new AboutPage(this);
    m_stack->addWidget(m_overview);
    m_stack->addWidget(m_injectPage);
    m_stack->addWidget(m_logsPage);
    m_stack->addWidget(m_settingsPage);
    m_stack->addWidget(m_aboutPage);

    m_toasts = new ToastHost(this);

    connect(m_sidebar, &Sidebar::currentChanged, this, &MainWindow::switchPage);

    // Start with the overview page.
    m_stack->setCurrentWidget(m_overview);

    resize(1080, 720);
}

void MainWindow::connectControllers()
{
    Settings* settings = Settings::instance();

    m_sequence = new InjectionSequence(this);
    m_monitor = new ProxyMonitor(this);
    m_monitor->setKeyword(settings->windowKeyword());
    m_monitor->setPort(AppPaths::defaultPort());
    m_logTail = new LogTail(this);

    // --- injection flow -----------------------------------------------------
    connect(m_injectPage, &InjectPage::startRequested, this, [this] { startInjection(); });
    connect(m_injectPage, &InjectPage::cancelRequested, this, [this] { cancelInjection(); });
    connect(m_injectPage, &InjectPage::restartAsAdminRequested, this,
            [this] { restartAsAdmin(); });

    connect(m_sequence, &InjectionSequence::stateChanged, this,
            [this](InjectionSequence::State state) {
                m_seqState = state;
                m_injectPage->onSequenceState(state);
                deriveStatus();
            });
    connect(m_sequence, &InjectionSequence::targetFound, this,
            [this](quint32 pid, const QString& title) {
                m_activePid = pid;
                m_injectPage->onTargetFound(pid, title);
            });
    connect(m_sequence, &InjectionSequence::waitPortProgress, this,
            [this](int elapsed, int total) { m_injectPage->onWaitPortProgress(elapsed, total); });
    connect(m_sequence, &InjectionSequence::finished, this,
            [this](bool ok, bool resumed, const QString& message) {
                m_injectPage->onFinished(ok, resumed, message);
                if (ok)
                {
                    if (m_activePid)
                        m_monitor->observe(m_activePid);
                    if (Settings::instance()->toastOnDone())
                        toast(Toast::Type::Success, message);
                    if (Settings::instance()->autoSwitchOnDone())
                        switchPage(QStringLiteral("overview"));
                }
                else
                {
                    toast(Toast::Type::Error, message);
                }
            });

    // --- monitoring -----------------------------------------------------------
    connect(m_monitor, &ProxyMonitor::snapshotUpdated, this,
            [this](const ProxySnapshot& snapshot) {
                m_snapshot = snapshot;
                m_overview->updateSnapshot(snapshot, m_seqState);
                deriveStatus();
            });

    // --- overview actions -----------------------------------------------------
    connect(m_overview, &OverviewPage::startRequested, this, [this] {
        switchPage(QStringLiteral("inject"));
        startInjection();
    });
    connect(m_overview, &OverviewPage::viewLogsRequested, this,
            [this] { switchPage(QStringLiteral("logs")); });

    // --- logs -----------------------------------------------------------------
    connect(m_logTail, &LogTail::sourceChanged, m_logsPage, &LogsPage::setSourcePath);
    connect(m_logTail, &LogTail::linesAppended, m_logsPage, &LogsPage::appendLines);
    connect(m_logTail, &LogTail::logReset, m_logsPage, &LogsPage::resetLines);
    connect(m_logsPage, &LogsPage::rescanRequested, m_logTail, &LogTail::rescan);
    m_logTail->rescan(); // replay the initial tail now that receivers are connected
    m_logsPage->setFollowEnabled(settings->followLog());

    // --- settings ---------------------------------------------------------------
    connect(m_settingsPage, &SettingsPage::restartAsAdminRequested, this,
            [this] { restartAsAdmin(); });
    connect(m_settingsPage, &SettingsPage::openSettingsDirRequested, this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::settingsDir()));
    });
    connect(m_settingsPage, &SettingsPage::resetRequested, this, [this] {
        Settings::instance()->resetAll();
        Theme::setPref(Theme::Pref::System);
        Theme::setBackdrop(Theme::Backdrop::None);
        m_monitor->setKeyword(Settings::instance()->windowKeyword());
        m_injectPage->reloadSettings();
        m_logTail->rescan();
        m_logsPage->reloadFromSettings();
        toast(Toast::Type::Info, QStringLiteral("已恢复默认设置"));
    });

    connect(settings, &Settings::themePrefChanged, this, [this] {
        Settings* s = Settings::instance();
        switch (s->themePref())
        {
        case Settings::ThemePref::Light:
            Theme::setPref(Theme::Pref::Light);
            break;
        case Settings::ThemePref::Dark:
            Theme::setPref(Theme::Pref::Dark);
            break;
        default:
            Theme::setPref(Theme::Pref::System);
            break;
        }
    });
    connect(settings, &Settings::backdropChanged, this, [this] {
        Settings* s = Settings::instance();
        switch (s->backdrop())
        {
        case Settings::Backdrop::Mica:
            Theme::setBackdrop(Theme::Backdrop::Mica);
            break;
        case Settings::Backdrop::Acrylic:
            Theme::setBackdrop(Theme::Backdrop::Acrylic);
            break;
        default:
            Theme::setBackdrop(Theme::Backdrop::None);
            break;
        }
        recreateForBackdrop(); // Theme::changed already repaints the widgets
    });
    connect(settings, &Settings::injectionSettingsChanged, this, [this] {
        m_monitor->setKeyword(Settings::instance()->windowKeyword());
    });
    connect(settings, &Settings::logSettingsChanged, this, [this] {
        m_logTail->rescan();
        m_logsPage->reloadFromSettings();
    });

    // --- theme propagation -------------------------------------------------------
    connect(Theme::instance(), &Theme::changed, this, [this] {
        applyGlobalTheme();
        applyNativeEffects();
    });
}

// ---------------------------------------------------------------------------
// Actions

void MainWindow::switchPage(const QString& id)
{
    QWidget* page = nullptr;
    if (id == QStringLiteral("overview"))
        page = m_overview;
    else if (id == QStringLiteral("inject"))
        page = m_injectPage;
    else if (id == QStringLiteral("logs"))
        page = m_logsPage;
    else if (id == QStringLiteral("settings"))
        page = m_settingsPage;
    else if (id == QStringLiteral("about"))
        page = m_aboutPage;
    if (!page || m_stack->currentWidget() == page)
        return;

    m_sidebar->setCurrentId(id);
    m_stack->setCurrentWidget(page);

    // Subtle fade-in for the new page.
    auto* effect = new QGraphicsOpacityEffect(page);
    page->setGraphicsEffect(effect);
    effect->setOpacity(0.0);
    auto* anim = new QPropertyAnimation(effect, "opacity", page);
    anim->setDuration(150);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    connect(anim, &QAbstractAnimation::finished, page, [page] { page->setGraphicsEffect(nullptr); });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::toast(Toast::Type type, const QString& text)
{
    m_toasts->showToast(type, text);
}

void MainWindow::startInjection()
{
    if (m_sequence->isRunning())
    {
        switchPage(QStringLiteral("inject"));
        return;
    }

    const QString dll = m_injectPage->dllPath();
    if (!Injector::fileReadable(dll))
    {
        toast(Toast::Type::Error, QStringLiteral("代理 DLL 不可用，请先在「注入」页选择文件"));
        switchPage(QStringLiteral("inject"));
        return;
    }

    InjectionSequence::Config config;
    config.dllPath = dll;
    config.keyword = m_injectPage->keyword();
    config.port = AppPaths::defaultPort();
    m_monitor->setKeyword(config.keyword);
    m_sequence->start(config);
}

void MainWindow::cancelInjection()
{
    m_sequence->cancel();
}

void MainWindow::restartAsAdmin()
{
    const QString exe = QCoreApplication::applicationFilePath();
    const HINSTANCE result = ShellExecuteW(
        nullptr, L"runas", reinterpret_cast<const wchar_t*>(exe.utf16()), nullptr, nullptr,
        SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) > 32)
    {
        close(); // elevated copy takes over
    }
    else
    {
        toast(Toast::Type::Info, QStringLiteral("已取消管理员授权"));
    }
}

void MainWindow::deriveStatus()
{
    Sidebar::StatusDot dot = Sidebar::StatusDot::Inactive;
    QString text = QStringLiteral("未运行");

    switch (m_seqState)
    {
    case InjectionSequence::State::WaitWindow:
        dot = Sidebar::StatusDot::Working;
        text = QStringLiteral("等待目标窗口");
        break;
    case InjectionSequence::State::Injecting:
        dot = Sidebar::StatusDot::Working;
        text = QStringLiteral("正在注入");
        break;
    case InjectionSequence::State::WaitPort:
        dot = Sidebar::StatusDot::Working;
        text = QStringLiteral("等待端口就绪");
        break;
    default:
        break;
    }

    if (m_seqState == InjectionSequence::State::Idle || m_seqState == InjectionSequence::State::Done
        || m_seqState == InjectionSequence::State::Failed
        || m_seqState == InjectionSequence::State::Cancelled)
    {
        const ProxySnapshot& s = m_snapshot;
        if (s.observing && s.alive && s.listening)
        {
            dot = Sidebar::StatusDot::Active;
            text = QStringLiteral("代理运行中");
        }
        else if (s.observing && s.alive && !s.listening && s.wasActive)
        {
            dot = Sidebar::StatusDot::Suspended;
            text = QStringLiteral("代理已停用");
        }
        else if (s.observing && !s.alive)
        {
            dot = Sidebar::StatusDot::Inactive;
            text = QStringLiteral("目标进程已退出");
        }
        else if (s.windowFound)
        {
            dot = Sidebar::StatusDot::Inactive;
            text = QStringLiteral("A 客户端已运行");
        }
        else if (m_seqState == InjectionSequence::State::Failed)
        {
            dot = Sidebar::StatusDot::Error;
            text = QStringLiteral("注入失败");
        }
    }

    m_sidebar->setStatus(dot, text);
}

// ---------------------------------------------------------------------------
// Theme

void MainWindow::applyGlobalTheme()
{
    qApp->setPalette(Theme::buildPalette());
    qApp->setStyleSheet(Theme::globalQss());
    const auto widgets = QApplication::allWidgets();
    for (QWidget* w : widgets)
        w->update();
}

// ---------------------------------------------------------------------------
// Native window frame

void MainWindow::setupNativeWindow()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());

    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    style &= ~LONG_PTR(WS_CAPTION);
    style |= LONG_PTR(WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    SetWindowLongPtrW(hwnd, GWL_STYLE, style);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    applyNativeEffects();
    m_nativeReady = true;
}

void MainWindow::applyNativeEffects()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());

    // Rounded corners (Windows 11+; harmless no-op earlier).
    DWORD corner = kDwcpRound;
    DwmSetWindowAttribute(hwnd, kDwmaWindowCornerPreference, &corner, sizeof(corner));

    // Match the native frame/system menu to the app theme.
    const BOOL dark = Theme::currentMode() == Theme::Mode::Dark ? TRUE : FALSE;
    if (FAILED(DwmSetWindowAttribute(hwnd, kDwmaImmersiveDarkMode, &dark, sizeof(dark))))
    {
        const DWORD legacyAttribute = 19;
        DwmSetWindowAttribute(hwnd, legacyAttribute, &dark, sizeof(dark));
    }

    // Optional Mica/Acrylic backdrop (Windows 11 22H2+).
    DWORD backdrop = kDwmsbtNone;
    if (Theme::translucentWindow())
        backdrop = Theme::backdrop() == Theme::Backdrop::Acrylic ? kDwmsbtAcrylic : kDwmsbtMica;
    DwmSetWindowAttribute(hwnd, kDwmaSystemBackdropType, &backdrop, sizeof(backdrop));
}

void MainWindow::recreateForBackdrop()
{
    const bool wantTranslucent = Theme::translucentWindow();
    const bool isTranslucent = testAttribute(Qt::WA_TranslucentBackground);
    if (wantTranslucent == isTranslucent)
    {
        applyNativeEffects(); // just flip the DWM attribute
        return;
    }

    // The translucency attribute must be set before the native window is
    // (re)created, so rebuild the window handle.
    const bool wasVisible = isVisible();
    const QRect geometry = frameGeometry();

    if (wasVisible)
        setVisible(false);
    if (windowHandle())
        windowHandle()->destroy();

    setAttribute(Qt::WA_TranslucentBackground, wantTranslucent);
    setAttribute(Qt::WA_NoSystemBackground, wantTranslucent);

    m_nativeReady = false;
    setGeometry(geometry);
    setupNativeWindow();
    if (wasVisible)
    {
        setVisible(true);
        raise();
        activateWindow();
    }
}

// ---------------------------------------------------------------------------
// Events

void MainWindow::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const auto& pal = Theme::pal();
    QColor bg = pal.windowBg;
    if (Theme::translucentWindow())
        bg = Theme::wash(pal.windowBg, 0.72);
    p.fillRect(rect(), bg);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    const int w = width();
    const int h = height();
    const int sw = Theme::SidebarWidth;

    m_sidebar->setGeometry(0, 0, sw, h);
    m_stack->setGeometry(sw, 0, w - sw, h);
    m_band->setGeometry(0, 0, w, Theme::TitleBarHeight);
    m_toasts->setGeometry(0, 0, w, h);

    m_toasts->raise();
    m_band->raise();
}

void MainWindow::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::ActivationChange)
        m_band->setWindowActive(isActiveWindow());
    else if (event->type() == QEvent::WindowStateChange)
        m_band->setMaximized(isMaximized());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    Settings::instance()->saveWindowGeometry(saveGeometry());
    m_sequence->shutdown();
    QWidget::closeEvent(event);
}

// ---------------------------------------------------------------------------
// nativeEvent: frameless-but-native behaviour

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    if (eventType != QByteArrayLiteral("windows_generic_msg"))
        return QWidget::nativeEvent(eventType, message, result);

    auto* msg = reinterpret_cast<MSG*>(message);
    const HWND hwnd = msg->hwnd;

    switch (msg->message)
    {
    case WM_NCCALCSIZE:
    {
        if (msg->wParam == TRUE)
        {
            auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);
            if (IsZoomed(hwnd))
            {
                // Keep the content inside the monitor when maximized
                // (the window rect extends past the screen edges by the frame).
                const int frameX =
                    GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                const int frameY =
                    GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                params->rgrc[0].left += frameX;
                params->rgrc[0].top += frameY;
                params->rgrc[0].right -= frameX;
                params->rgrc[0].bottom -= frameY;
            }
            *result = 0; // client area == window rect
            return true;
        }
        break;
    }

    case WM_NCHITTEST:
    {
        const POINT cursor{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};

        RECT wr;
        GetWindowRect(hwnd, &wr);

        // Client-origin coordinates (account for the maximized inset).
        const bool zoomed = IsZoomed(hwnd);
        int insetX = 0, insetY = 0;
        if (zoomed)
        {
            insetX = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
            insetY = GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
        }
        const int x = int(cursor.x - wr.left) - insetX;
        const int y = int(cursor.y - wr.top) - insetY;
        const int w = width();
        const int h = height();

        // Resize borders (hidden while maximized).
        if (!zoomed)
        {
            const bool left = x < kResizeBorder;
            const bool right = w - kResizeBorder <= x;
            const bool top = y < kResizeBorder;
            const bool bottom = h - kResizeBorder <= y;
            if (top && left)
            {
                *result = HTTOPLEFT;
                return true;
            }
            if (top && right)
            {
                *result = HTTOPRIGHT;
                return true;
            }
            if (bottom && left)
            {
                *result = HTBOTTOMLEFT;
                return true;
            }
            if (bottom && right)
            {
                *result = HTBOTTOMRIGHT;
                return true;
            }
            if (left)
            {
                *result = HTLEFT;
                return true;
            }
            if (right)
            {
                *result = HTRIGHT;
                return true;
            }
            if (top)
            {
                *result = HTTOP;
                return true;
            }
            if (bottom)
            {
                *result = HTBOTTOM;
                return true;
            }
        }

        // Caption band; the traffic lights stay interactive.
        if (y < Theme::TitleBarHeight)
        {
            const QList<QRect> holes = m_band->trafficHitRects();
            for (const QRect& r : holes)
            {
                if (r.contains(x, y))
                {
                    *result = HTCLIENT; // deliver to Qt (traffic light button)
                    return true;
                }
            }
            *result = HTCAPTION; // native drag / double-click / right-click menu
            return true;
        }
        break;
    }

    default:
        break;
    }

    return QWidget::nativeEvent(eventType, message, result);
}
