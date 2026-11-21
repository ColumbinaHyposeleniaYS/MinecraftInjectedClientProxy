#include "pages/InjectPage.h"

#include "app/Buttons.h"
#include "app/Card.h"
#include "app/LineEdit.h"
#include "app/ProgressBar.h"
#include "app/Stepper.h"
#include "app/Theme.h"
#include "core/AppPaths.h"
#include "core/Settings.h"

#include <QFile>
#include <QHBoxLayout>
#include <QPainter>
#include <QVBoxLayout>

#include <windows.h>
#include <shellapi.h>

namespace {

// Result banner: rounded translucent surface, recoloured by outcome.
class ResultBanner : public QWidget
{
public:
    enum class Kind { Hidden, Success, Error, Warning };

    explicit ResultBanner(QWidget* parent)
        : QWidget(parent)
    {
        m_title = new StyledLabel(StyledLabel::Role::Primary, QString(), 13, 600, this);
        m_detail = new StyledLabel(StyledLabel::Role::Secondary, QString(), 12, 400, this);
        m_detail->setWordWrap(true);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(4);
        layout->addWidget(m_title);
        layout->addWidget(m_detail);

        connect(Theme::instance(), &Theme::changed, this, [this] { update(); });
        hide();
    }

    void show(Kind kind, const QString& title, const QString& detail)
    {
        m_kind = kind;
        m_title->setText(title);
        m_detail->setText(detail);
        switch (kind)
        {
        case Kind::Success:
            m_title->setRole(StyledLabel::Role::Success);
            break;
        case Kind::Error:
            m_title->setRole(StyledLabel::Role::Danger);
            break;
        case Kind::Warning:
            m_title->setRole(StyledLabel::Role::Warning);
            break;
        default:
            break;
        }
        QWidget::show();
        update();
    }

    void hideBanner()
    {
        m_kind = Kind::Hidden;
        hide();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const auto& pal = Theme::pal();

        QColor tint;
        switch (m_kind)
        {
        case Kind::Success:
            tint = pal.successSoft;
            break;
        case Kind::Error:
            tint = pal.dangerSoft;
            break;
        case Kind::Warning:
            tint = pal.warningSoft;
            break;
        default:
            return;
        }

        const QRectF rect = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        p.setPen(Qt::NoPen);
        p.setBrush(tint);
        p.drawRoundedRect(rect, 10, 10);
    }

private:
    StyledLabel* m_title = nullptr;
    StyledLabel* m_detail = nullptr;
    Kind m_kind = Kind::Hidden;
};

// A labelled settings row: fixed-width label column, stretchy control.
QWidget* fieldRow(const QString& caption, const QString& hint, QWidget* control,
                  QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    auto* textHost = new QWidget(row);
    textHost->setFixedWidth(264);
    auto* text = new QVBoxLayout(textHost);
    text->setContentsMargins(0, 0, 0, 0);
    text->setSpacing(2);
    text->addWidget(new StyledLabel(StyledLabel::Role::Primary, caption, 13, 500, textHost));
    if (!hint.isEmpty())
        text->addWidget(new StyledLabel(StyledLabel::Role::Tertiary, hint, 11, 400, textHost));

    layout->addWidget(textHost, 0, Qt::AlignVCenter);
    layout->addWidget(control, 1, Qt::AlignVCenter);
    return row;
}

} // namespace

// ---------------------------------------------------------------------------

InjectPage::InjectPage(QWidget* parent)
    : Page(QStringLiteral("注入"), QStringLiteral("选择代理 DLL 并开始一次注入流程"), parent)
{
    auto* content = contentLayout();

    // --- admin banner --------------------------------------------------------
    if (!runningAsAdmin())
        buildAdminBanner();

    // --- configuration card ----------------------------------------------------
    auto* config = new Card(this);
    auto* configTitle = new StyledLabel(StyledLabel::Role::Primary, QStringLiteral("注入配置"), 15,
                                        600, config);
    config->contentLayout()->addWidget(configTitle);

    m_dll = new BrowseField(config);
    m_dll->setMode(BrowseField::Mode::DllFile);
    m_dll->setDialogTitle(QStringLiteral("选择代理 DLL"));
    m_dll->setPlaceholder(QStringLiteral("Aurora.dll 的路径"));
    config->contentLayout()->addWidget(
        fieldRow(QStringLiteral("代理 DLL"), QStringLiteral("随代理引擎构建产出的反射式 DLL"),
                 m_dll, config));

    m_keyword = new LineEdit(config);
    m_keyword->setPlaceholder(QStringLiteral("窗口标题包含的关键字"));
    config->contentLayout()->addWidget(
        fieldRow(QStringLiteral("窗口关键字"),
                 QStringLiteral("注入器据此定位目标 Java 窗口（默认“布吉岛”，可在服务端调整）"),
                 m_keyword, config));

    auto* portRow = new StyledLabel(
        StyledLabel::Role::Tertiary,
        QStringLiteral("监听端口 25565 —— 由代理 DLL 在 A 进程内开启，B 客户端使用「直接连接」加入"),
        11, 400, config);
    config->contentLayout()->addWidget(portRow);

    content->addWidget(config);

    // --- progress card ----------------------------------------------------------
    auto* runCard = new Card(this);
    auto* runTitle =
        new StyledLabel(StyledLabel::Role::Primary, QStringLiteral("注入状态"), 15, 600, runCard);
    runCard->contentLayout()->addWidget(runTitle);

    m_steps = new Stepper(runCard);
    m_steps->setSteps({QStringLiteral("等待目标窗口"), QStringLiteral("匹配目标进程"),
                       QStringLiteral("注入代理 DLL"), QStringLiteral("等待端口就绪"),
                       QStringLiteral("完成")});
    runCard->contentLayout()->addWidget(m_steps);

    m_progress = new ProgressBar(runCard);
    m_progress->setRange(0, 30000);
    m_progress->reset();
    m_progress->hide();
    runCard->contentLayout()->addWidget(m_progress);

    m_resultBanner = new ResultBanner(runCard);
    runCard->contentLayout()->addWidget(m_resultBanner);

    auto* actions = new QWidget(runCard);
    auto* actionsLayout = new QHBoxLayout(actions);
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(10);
    m_start = new AppButton(QStringLiteral("开始注入"), AppButton::Style::Primary, actions);
    m_start->setIcon(Icons::Icon::Play);
    m_cancel = new AppButton(QStringLiteral("取消"), AppButton::Style::Secondary, actions);
    m_cancel->hide();
    auto* noteLabel = new StyledLabel(
        StyledLabel::Role::Tertiary,
        QStringLiteral("若代理已驻留，再次注入将直接发送恢复信号，不会重复加载 DLL"), 11, 400,
        actions);
    actionsLayout->addWidget(m_start);
    actionsLayout->addWidget(m_cancel);
    actionsLayout->addSpacing(8);
    actionsLayout->addWidget(noteLabel, 1);
    runCard->contentLayout()->addWidget(actions);

    content->addWidget(runCard);
    content->addStretch(1);

    connect(m_start, &QAbstractButton::clicked, this, [this] {
        Settings::instance()->setDllPath(m_dll->text());
        Settings::instance()->setWindowKeyword(m_keyword->text());
        emit startRequested();
    });
    connect(m_cancel, &QAbstractButton::clicked, this, &InjectPage::cancelRequested);
    connect(m_dll, &BrowseField::textEdited, this, [](const QString& text) {
        Settings::instance()->setDllPath(text);
    });
    connect(m_keyword, &QLineEdit::textEdited, this, [](const QString& text) {
        Settings::instance()->setWindowKeyword(text);
    });

    reloadSettings();
}

void InjectPage::buildAdminBanner()
{
    auto* banner = new Card(this);
    banner->setPadding(14);

    auto* layout = new QHBoxLayout(banner);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* icon = new IconTile(32, 18, banner);
    icon->setIcon(Icons::Icon::Shield);
    icon->setTint(IconTile::Tint::Warning);

    auto* text = new StyledLabel(StyledLabel::Role::Secondary,
                                 QStringLiteral("当前以普通权限运行。若游戏以管理员身份运行，注入需要相同权限。"),
                                 12, 400, banner);
    text->setWordWrap(true);

    auto* elevate = new AppButton(QStringLiteral("以管理员身份重启"), AppButton::Style::Secondary,
                                  banner);
    connect(elevate, &QAbstractButton::clicked, this, &InjectPage::restartAsAdminRequested);

    layout->addWidget(icon, 0, Qt::AlignVCenter);
    layout->addWidget(text, 1);
    layout->addWidget(elevate, 0, Qt::AlignVCenter);

    contentLayout()->addWidget(banner);
}

bool InjectPage::runningAsAdmin()
{
    BOOL admin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup))
    {
        if (!CheckTokenMembership(nullptr, adminGroup, &admin))
            admin = FALSE;
        FreeSid(adminGroup);
    }
    return admin == TRUE;
}

QString InjectPage::dllPath() const
{
    return m_dll->text();
}

QString InjectPage::keyword() const
{
    return m_keyword->text();
}

void InjectPage::reloadSettings()
{
    Settings* settings = Settings::instance();

    QString dll = settings->dllPath();
    if (dll.isEmpty())
    {
        for (const QString& candidate : AppPaths::defaultDllCandidates())
        {
            if (QFile::exists(candidate))
            {
                dll = candidate;
                break;
            }
        }
    }
    m_dll->setText(dll);
    m_keyword->setText(settings->windowKeyword());
}

void InjectPage::onSequenceState(InjectionSequence::State state)
{
    m_cancel->setVisible(state == InjectionSequence::State::WaitWindow
                         || state == InjectionSequence::State::WaitPort);
    m_start->setEnabled(state == InjectionSequence::State::Idle
                        || state == InjectionSequence::State::Done
                        || state == InjectionSequence::State::Failed
                        || state == InjectionSequence::State::Cancelled);

    switch (state)
    {
    case InjectionSequence::State::WaitWindow:
        m_steps->resetStates();
        m_steps->setState(0, Stepper::StepState::Active, QStringLiteral("等待中…"));
        m_progress->hide();
        m_progress->reset();
        static_cast<ResultBanner*>(m_resultBanner)->hideBanner();
        break;
    case InjectionSequence::State::Injecting:
        m_steps->setState(0, Stepper::StepState::Done);
        m_steps->setState(1, Stepper::StepState::Done);
        m_steps->setState(2, Stepper::StepState::Active);
        break;
    case InjectionSequence::State::WaitPort:
        m_steps->setState(2, Stepper::StepState::Done);
        m_steps->setState(3, Stepper::StepState::Active, QStringLiteral("最长等待 30 秒"));
        m_progress->show();
        m_progress->setRange(0, 30000);
        m_progress->reset();
        break;
    default:
        break;
    }
}

void InjectPage::onTargetFound(quint32 pid, const QString& title)
{
    m_steps->setState(0, Stepper::StepState::Done);
    m_steps->setState(1, Stepper::StepState::Done,
                      QStringLiteral("PID %1 · %2")
                          .arg(pid)
                          .arg(title.left(48) + (title.size() > 48 ? QStringLiteral("…")
                                                                  : QString())));
    m_steps->setState(2, Stepper::StepState::Active);
}

void InjectPage::onWaitPortProgress(int elapsedMs, int totalMs)
{
    m_progress->setRange(0, totalMs);
    m_progress->setValue(elapsedMs);
    m_steps->setState(3, Stepper::StepState::Active,
                      QStringLiteral("已等待 %1 秒 / 最长 30 秒").arg(elapsedMs / 1000));
}

void InjectPage::onFinished(bool ok, bool resumed, const QString& message)
{
    m_progress->hide();

    auto* banner = static_cast<ResultBanner*>(m_resultBanner);
    if (ok)
    {
        m_steps->setState(3, Stepper::StepState::Done);
        m_steps->setState(4, Stepper::StepState::Done, resumed ? QStringLiteral("已恢复驻留代理")
                                                               : QStringLiteral("端口 25565 就绪"));
        banner->show(ResultBanner::Kind::Success, resumed ? QStringLiteral("代理已恢复")
                                                          : QStringLiteral("注入完成"),
                     QStringLiteral("%1。B 客户端现在可以通过 127.0.0.1:25565 加入。").arg(message));
    }
    else
    {
        m_steps->failActive(message);
        banner->show(ResultBanner::Kind::Error, QStringLiteral("注入未完成"), message);
    }
}
