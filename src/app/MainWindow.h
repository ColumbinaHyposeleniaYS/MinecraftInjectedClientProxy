#pragma once

#include "app/Sidebar.h"
#include "app/Toast.h"
#include "core/InjectionSequence.h"
#include "core/ProxyMonitor.h"

#include <QStackedWidget>
#include <QWidget>

class AboutPage;
class InjectPage;
class LogTail;
class LogsPage;
class OverviewPage;
class ProxyMonitor;
class SettingsPage;
class TitleBand;

/*
 * Frameless main window.
 *
 * Native side (see nativeEvent):
 *   - WS_CAPTION is stripped while WS_THICKFRAME/WS_SYSMENU are kept, which
 *     preserves native resize, Aero Snap, the DWM shadow and the right-click
 *     system menu.
 *   - WM_NCCALCSIZE makes the client area cover the whole window.
 *   - WM_NCHITTEST carves out resize borders, a 48 px caption band and
 *     HTCLIENT holes for the traffic light buttons.
 *   - DWM attributes add rounded corners (Win11), the immersive dark frame
 *     and the optional Mica/Acrylic backdrop.
 *
 * Qt side: TitleBand (traffic lights), 220 px Sidebar, a QStackedWidget with
 * the five pages and the toast host.
 */
class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow();
    ~MainWindow() override;

    void switchPage(const QString& id);
    void toast(Toast::Type type, const QString& text);

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void connectControllers();
    void setupNativeWindow();
    void applyNativeEffects();
    void applyGlobalTheme();
    void recreateForBackdrop();

    void startInjection();
    void cancelInjection();
    void restartAsAdmin();

    void deriveStatus();

    TitleBand* m_band = nullptr;
    Sidebar* m_sidebar = nullptr;
    QStackedWidget* m_stack = nullptr;
    ToastHost* m_toasts = nullptr;

    OverviewPage* m_overview = nullptr;
    InjectPage* m_injectPage = nullptr;
    LogsPage* m_logsPage = nullptr;
    SettingsPage* m_settingsPage = nullptr;
    AboutPage* m_aboutPage = nullptr;

    InjectionSequence* m_sequence = nullptr;
    ProxyMonitor* m_monitor = nullptr;
    LogTail* m_logTail = nullptr;

    ProxySnapshot m_snapshot;
    DWORD m_activePid = 0;
    InjectionSequence::State m_seqState = InjectionSequence::State::Idle;
    bool m_nativeReady = false;
    bool m_geometryRestored = false;
};
