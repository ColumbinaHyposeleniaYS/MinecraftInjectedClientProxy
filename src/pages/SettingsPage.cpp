#include "pages/SettingsPage.h"

#include "app/Buttons.h"
#include "app/Card.h"
#include "app/LineEdit.h"
#include "app/SegmentedControl.h"
#include "app/Theme.h"
#include "app/ToggleSwitch.h"
#include "core/AppPaths.h"
#include "core/Settings.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>

namespace {

// Row with an optional bottom hairline separator.
class Row : public QWidget
{
public:
    explicit Row(QWidget* parent) : QWidget(parent) { setAttribute(Qt::WA_StyledBackground, false); }
    void setSeparatorEnabled(bool on) { m_separator = on; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (!m_separator)
            return;
        QPainter p(this);
        const auto& pal = Theme::pal();
        p.fillRect(QRect(0, height() - 1, width(), 1), pal.separator);
    }

private:
    bool m_separator = true;
};

} // namespace

SettingsPage::SettingsPage(QWidget* parent)
    : Page(QStringLiteral("设置"), QStringLiteral("外观、注入与日志偏好"), parent)
{
    auto* content = contentLayout();
    Settings* settings = Settings::instance();

    // --- appearance ---------------------------------------------------------
    QWidget* appearance = sectionCard(QStringLiteral("外观"));

    m_theme = new SegmentedControl(appearance);
    m_theme->addItem(QStringLiteral("system"), QStringLiteral("跟随系统"));
    m_theme->addItem(QStringLiteral("light"), QStringLiteral("浅色"));
    m_theme->addItem(QStringLiteral("dark"), QStringLiteral("深色"));
    switch (settings->themePref())
    {
    case Settings::ThemePref::Light:
        m_theme->setCurrentId(QStringLiteral("light"));
        break;
    case Settings::ThemePref::Dark:
        m_theme->setCurrentId(QStringLiteral("dark"));
        break;
    default:
        m_theme->setCurrentId(QStringLiteral("system"));
        break;
    }
    appearance->layout()->addWidget(row(QStringLiteral("主题"), QStringLiteral("浅色 #F5F5F7 · 深色跟随系统"), m_theme));

    m_backdrop = new SegmentedControl(appearance);
    m_backdrop->addItem(QStringLiteral("none"), QStringLiteral("关闭"));
    m_backdrop->addItem(QStringLiteral("mica"), QStringLiteral("Mica"));
    m_backdrop->addItem(QStringLiteral("acrylic"), QStringLiteral("亚克力"));
    switch (settings->backdrop())
    {
    case Settings::Backdrop::Mica:
        m_backdrop->setCurrentId(QStringLiteral("mica"));
        break;
    case Settings::Backdrop::Acrylic:
        m_backdrop->setCurrentId(QStringLiteral("acrylic"));
        break;
    default:
        m_backdrop->setCurrentId(QStringLiteral("none"));
        break;
    }
    if (!Theme::backdropSupported())
    {
        m_backdrop->setItemEnabled(QStringLiteral("mica"), false);
        m_backdrop->setItemEnabled(QStringLiteral("acrylic"), false);
        m_backdropHint = new StyledLabel(StyledLabel::Role::Tertiary,
                                         QStringLiteral("当前系统不支持 Mica / 亚克力（需要 Windows 11 22H2 或更高版本）"),
                                         11, 400, appearance);
    }
    QWidget* backdropRow = row(QStringLiteral("窗口材质"),
                              QStringLiteral("通过 DWM 应用背景材质，需要重启窗口生效"), m_backdrop);
    appearance->layout()->addWidget(backdropRow);
    if (m_backdropHint)
        appearance->layout()->addWidget(m_backdropHint);

    connect(m_theme, &SegmentedControl::currentChanged, this, [](const QString& id) {
        if (id == QStringLiteral("light"))
            Settings::instance()->setThemePref(Settings::ThemePref::Light);
        else if (id == QStringLiteral("dark"))
            Settings::instance()->setThemePref(Settings::ThemePref::Dark);
        else
            Settings::instance()->setThemePref(Settings::ThemePref::System);
    });
    connect(m_backdrop, &SegmentedControl::currentChanged, this, [](const QString& id) {
        if (id == QStringLiteral("mica"))
            Settings::instance()->setBackdrop(Settings::Backdrop::Mica);
        else if (id == QStringLiteral("acrylic"))
            Settings::instance()->setBackdrop(Settings::Backdrop::Acrylic);
        else
            Settings::instance()->setBackdrop(Settings::Backdrop::None);
    });

    content->addWidget(appearance);

    // --- injection -----------------------------------------------------------
    QWidget* injection = sectionCard(QStringLiteral("注入"));

    m_dll = new BrowseField(injection);
    m_dll->setMode(BrowseField::Mode::DllFile);
    m_dll->setDialogTitle(QStringLiteral("选择代理 DLL"));
    m_dll->setPlaceholder(QStringLiteral("默认在程序目录探测 Aurora.dll"));
    m_dll->setText(settings->dllPath());
    injection->layout()->addWidget(
        row(QStringLiteral("代理 DLL"), QStringLiteral("反射式注入到 A 客户端进程的 DLL"), m_dll));

    m_keyword = new LineEdit(injection);
    m_keyword->setText(settings->windowKeyword());
    injection->layout()->addWidget(row(
        QStringLiteral("窗口关键字"),
        QStringLiteral("标题包含该关键字的 Java 窗口被视为目标（默认“布吉岛”）"), m_keyword));

    m_autoSwitch = new ToggleSwitch(injection);
    m_autoSwitch->setChecked(settings->autoSwitchOnDone());
    injection->layout()->addWidget(
        row(QStringLiteral("完成后跳转总览"),
            QStringLiteral("注入成功后自动切换到「总览」页"), m_autoSwitch));

    m_toastOnDone = new ToggleSwitch(injection);
    m_toastOnDone->setChecked(settings->toastOnDone());
    injection->layout()->addWidget(
        row(QStringLiteral("完成通知"), QStringLiteral("注入成功时显示浮动通知"), m_toastOnDone, false));

    connect(m_dll, &BrowseField::textEdited, this,
            [](const QString& text) { Settings::instance()->setDllPath(text); });
    connect(m_keyword, &QLineEdit::textEdited, this,
            [](const QString& text) { Settings::instance()->setWindowKeyword(text); });
    connect(m_autoSwitch, &QAbstractButton::toggled, this, [](bool on) {
        Settings::instance()->setAutoSwitchOnDone(on);
    });
    connect(m_toastOnDone, &QAbstractButton::toggled, this, [](bool on) {
        Settings::instance()->setToastOnDone(on);
    });

    content->addWidget(injection);

    // --- logs ---------------------------------------------------------------
    QWidget* logsCard = sectionCard(QStringLiteral("日志"));

    m_logOverride = new BrowseField(logsCard);
    m_logOverride->setMode(BrowseField::Mode::LogFile);
    m_logOverride->setDialogTitle(QStringLiteral("选择日志文件"));
    m_logOverride->setPlaceholder(QStringLiteral("留空则按顺序自动探测"));
    m_logOverride->setText(settings->logPathOverride());
    logsCard->layout()->addWidget(
        row(QStringLiteral("日志文件"),
            QStringLiteral("探测顺序：.minecraft\\proxy.log → TEMP → C:\\（留空自动）"), m_logOverride,
            false));

    connect(m_logOverride, &BrowseField::textEdited, this, [](const QString& text) {
        Settings::instance()->setLogPathOverride(text);
    });

    content->addWidget(logsCard);

    // --- advanced --------------------------------------------------------------
    QWidget* advanced = sectionCard(QStringLiteral("高级"));

    auto* elevate = new AppButton(QStringLiteral("以管理员身份重启"), AppButton::Style::Secondary,
                                  advanced);
    elevate->setIcon(Icons::Icon::Shield);
    advanced->layout()->addWidget(
        row(QStringLiteral("权限"), QStringLiteral("游戏以管理员运行时，本程序需要相同权限"), elevate));

    auto* openDir =
        new AppButton(QStringLiteral("打开设置目录"), AppButton::Style::Ghost, advanced);
    openDir->setIcon(Icons::Icon::Folder);
    advanced->layout()->addWidget(
        row(QStringLiteral("设置文件"),
            QStringLiteral("settings.ini 保存在 %APPDATA%\\MinecraftInjectedClientProxy"), openDir));

    m_reset = new AppButton(QStringLiteral("恢复默认设置"), AppButton::Style::GhostDanger, advanced);
    m_reset->setIcon(Icons::Icon::Refresh);
    advanced->layout()->addWidget(row(QString(), QString(), m_reset, false));

    connect(elevate, &QAbstractButton::clicked, this, &SettingsPage::restartAsAdminRequested);
    connect(openDir, &QAbstractButton::clicked, this, &SettingsPage::openSettingsDirRequested);
    connect(m_reset, &QAbstractButton::clicked, this, [this] {
        if (!m_resetArmed)
        {
            m_resetArmed = true;
            m_reset->setText(QStringLiteral("再次点击以确认"));
            QTimer::singleShot(3000, this, [this] {
                m_resetArmed = false;
                m_reset->setText(QStringLiteral("恢复默认设置"));
            });
        }
        else
        {
            m_resetArmed = false;
            m_reset->setText(QStringLiteral("恢复默认设置"));
            emit resetRequested();
        }
    });

    content->addWidget(advanced);
    content->addStretch(1);
}

QWidget* SettingsPage::sectionCard(const QString& title)
{
    auto* card = new Card(this);
    card->setPadding(20);
    card->contentLayout()->setSpacing(0);
    auto* label = new StyledLabel(StyledLabel::Role::Primary, title, 15, 600, card);
    card->contentLayout()->addWidget(label);
    card->contentLayout()->addSpacing(8);
    return card;
}

QWidget* SettingsPage::row(const QString& caption, const QString& hint, QWidget* control,
                           bool separator)
{
    auto* rowHost = new Row(this);
    rowHost->setSeparatorEnabled(separator);

    auto* layout = new QHBoxLayout(rowHost);
    layout->setContentsMargins(0, 14, 0, 15);
    layout->setSpacing(16);

    if (!caption.isEmpty() || !hint.isEmpty())
    {
        auto* textHost = new QWidget(rowHost);
        textHost->setFixedWidth(264);
        auto* text = new QVBoxLayout(textHost);
        text->setContentsMargins(0, 0, 0, 0);
        text->setSpacing(2);
        if (!caption.isEmpty())
            text->addWidget(
                new StyledLabel(StyledLabel::Role::Primary, caption, 13, 500, textHost));
        if (!hint.isEmpty())
            text->addWidget(new StyledLabel(StyledLabel::Role::Tertiary, hint, 11, 400, textHost));
        layout->addWidget(textHost, 0, Qt::AlignVCenter);
    }

    layout->addWidget(control, 1, Qt::AlignVCenter);

    return rowHost;
}
