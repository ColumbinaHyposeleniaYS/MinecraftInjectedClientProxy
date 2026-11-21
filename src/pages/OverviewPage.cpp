#include "pages/OverviewPage.h"

#include "app/Buttons.h"
#include "app/Card.h"
#include "app/Theme.h"
#include "core/AppPaths.h"
#include "core/InjectionSequence.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QVBoxLayout>

namespace {

// Small standalone icon glyph (theme aware).
class GlyphWidget : public QWidget
{
public:
    GlyphWidget(Icons::Icon icon, int extent, QWidget* parent)
        : QWidget(parent), m_icon(icon), m_extent(extent)
    {
        setFixedSize(extent, extent);
        connect(Theme::instance(), &Theme::changed, this, [this] { update(); });
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        Icons::paint(p, m_icon, QRectF(rect()), Theme::pal().textTertiary, 1.5);
    }

private:
    Icons::Icon m_icon;
    int m_extent;
};

QWidget* guideRow(int number, const QString& title, const QString& detail)
{
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    layout->addWidget(new NumberChip(number, row));

    auto* text = new QVBoxLayout;
    text->setContentsMargins(0, 0, 0, 0);
    text->setSpacing(2);
    text->addWidget(new StyledLabel(StyledLabel::Role::Primary, title, 13, 500));
    text->addWidget(new StyledLabel(StyledLabel::Role::Secondary, detail, 12, 400));
    layout->addLayout(text, 1);
    return row;
}

QWidget* noteRow(const QString& text)
{
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    layout->addWidget(new GlyphWidget(Icons::Icon::Info, 16, row), 0, Qt::AlignTop);
    layout->addWidget(new StyledLabel(StyledLabel::Role::Secondary, text, 12, 400), 1);
    return row;
}

// One stat card: icon tile + caption + value + sub caption.
struct StatCard
{
    Card* card = nullptr;
    IconTile* icon = nullptr;
    StyledLabel* caption = nullptr;
    StyledLabel* value = nullptr;
    StyledLabel* sub = nullptr;
};

StatCard makeStatCard(Icons::Icon icon, IconTile::Tint tint, const QString& caption,
                      QWidget* parent)
{
    StatCard sc;
    sc.card = new Card(parent);
    sc.card->setPadding(18);
    sc.card->contentLayout()->setSpacing(6);

    sc.icon = new IconTile(32, 18, sc.card);
    sc.icon->setIcon(icon);
    sc.icon->setTint(tint);

    sc.caption = new StyledLabel(StyledLabel::Role::Secondary, caption, 11, 500, sc.card);
    sc.value = new StyledLabel(StyledLabel::Role::Primary, QStringLiteral("—"), 15, 600,
                               sc.card);
    sc.sub = new StyledLabel(StyledLabel::Role::Tertiary, QString(), 11, 400, sc.card);

    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    head->setSpacing(10);
    head->addWidget(sc.icon);
    head->addWidget(sc.caption, 1);

    sc.card->contentLayout()->addLayout(head);
    sc.card->contentLayout()->addWidget(sc.value);
    sc.card->contentLayout()->addWidget(sc.sub);
    return sc;
}

} // namespace

// ---------------------------------------------------------------------------

OverviewPage::OverviewPage(QWidget* parent)
    : Page(QStringLiteral("总览"),
           QStringLiteral("查看代理、A/B 客户端与监听端口的实时状态"), parent)
{
    auto* content = contentLayout();

    // --- hero ---------------------------------------------------------------
    m_hero = new Card(this);
    m_hero->setPadding(24);

    m_heroIcon = new IconTile(52, 30, m_hero);
    m_heroIcon->setIcon(Icons::Icon::Cube);
    m_heroIcon->setTint(IconTile::Tint::Neutral);

    auto* heroText = new QVBoxLayout;
    heroText->setContentsMargins(0, 0, 0, 0);
    heroText->setSpacing(4);
    m_heroTitle = new StyledLabel(StyledLabel::Role::Primary, QStringLiteral("代理未运行"), 20,
                                  700, m_hero);
    m_heroSub = new StyledLabel(
        StyledLabel::Role::Secondary,
        QStringLiteral("点击「开始注入」启动一次注入流程；完成后 B 客户端可通过本机 25565 端口加入 A 的会话"),
        12, 400, m_hero);
    m_heroSub->setWordWrap(true);
    heroText->addWidget(m_heroTitle);
    heroText->addWidget(m_heroSub);

    m_ctaPrimary = new AppButton(QStringLiteral("开始注入"), AppButton::Style::Primary, m_hero);
    m_ctaPrimary->setIcon(Icons::Icon::Play);
    m_ctaSecondary = new AppButton(QStringLiteral("查看日志"), AppButton::Style::Secondary,
                                   m_hero);

    auto* heroLayout = new QHBoxLayout;
    heroLayout->setContentsMargins(0, 0, 0, 0);
    heroLayout->setSpacing(20);
    heroLayout->addWidget(m_heroIcon, 0, Qt::AlignTop);
    heroLayout->addLayout(heroText, 1);
    heroLayout->addSpacing(8);
    heroLayout->addWidget(m_ctaPrimary, 0, Qt::AlignVCenter);
    heroLayout->addWidget(m_ctaSecondary, 0, Qt::AlignVCenter);

    m_hero->contentLayout()->addLayout(heroLayout);
    content->addWidget(m_hero);

    // --- stat cards -----------------------------------------------------------
    auto* stats = new QWidget(this);
    auto* statsLayout = new QHBoxLayout(stats);
    statsLayout->setContentsMargins(0, 0, 0, 0);
    statsLayout->setSpacing(16);

    StatCard aCard =
        makeStatCard(Icons::Icon::User, IconTile::Tint::Accent, QStringLiteral("A 客户端"), stats);
    StatCard lCard = makeStatCard(Icons::Icon::Nodes, IconTile::Tint::Accent,
                                  QStringLiteral("代理监听"), stats);
    StatCard bCard = makeStatCard(Icons::Icon::Link, IconTile::Tint::Accent,
                                  QStringLiteral("B 客户端"), stats);

    m_aValue = aCard.value;
    m_aCaption = aCard.sub;
    m_listenerValue = lCard.value;
    m_listenerCaption = lCard.sub;
    m_bValue = bCard.value;
    m_bCaption = bCard.sub;

    statsLayout->addWidget(aCard.card, 1);
    statsLayout->addWidget(lCard.card, 1);
    statsLayout->addWidget(bCard.card, 1);
    content->addWidget(stats);

    // --- guide ---------------------------------------------------------------
    buildGuideCard();
    buildNotesCard();

    content->addStretch(1);

    connect(m_ctaPrimary, &QAbstractButton::clicked, this, &OverviewPage::startRequested);
    connect(m_ctaSecondary, &QAbstractButton::clicked, this, &OverviewPage::viewLogsRequested);
}

void OverviewPage::buildGuideCard()
{
    auto* card = new Card(this);
    auto* title = new StyledLabel(StyledLabel::Role::Primary, QStringLiteral("快速上手"), 15, 600,
                                  card);
    card->contentLayout()->addWidget(title);

    struct Guide
    {
        const char* title;
        const char* detail;
    };
    const Guide guides[] = {
        {"启动 A 客户端", "启动并进入目标服务器；也可以先跳过，采用“入服前注入”"},
        {"开始注入", "在「注入」页选择代理 DLL 后开始；支持 A 入服前与入服后两种方式"},
        {"B 加入会话", "B 客户端使用「直接连接」输入 127.0.0.1:25565（同机）或 A 的局域网 IP"},
        {"共享世界", "B 进入后即可操作；若 A 已在服务器内，B 会先同步 A 当前的世界"},
    };
    for (int i = 0; i < 4; ++i)
        card->contentLayout()->addWidget(
            guideRow(i + 1, QString::fromUtf8(guides[i].title), QString::fromUtf8(guides[i].detail)));

    contentLayout()->addWidget(card);
}

void OverviewPage::buildNotesCard()
{
    auto* card = new Card(this);
    auto* title =
        new StyledLabel(StyledLabel::Role::Primary, QStringLiteral("注意事项"), 15, 600, card);
    card->contentLayout()->addWidget(title);

    const QString notes[] = {
        QStringLiteral("仅支持 Minecraft Java 1.20.1，A、B 两个客户端版本均需匹配"),
        QStringLiteral("注入器需要与游戏进程同权限：游戏以管理员运行时，本程序也要以管理员运行"),
        QStringLiteral("A 窗口在前台时按 DEL 可随时停用代理；再次注入即可恢复"),
        QStringLiteral("B 以离线身份加入，不进行 Mojang 正版验证"),
    };
    for (const QString& note : notes)
        card->contentLayout()->addWidget(noteRow(note));

    contentLayout()->addWidget(card);
}

void OverviewPage::updateSnapshot(const ProxySnapshot& snapshot, InjectionSequence::State state)
{
    m_snapshot = snapshot;
    m_seqState = int(state);

    // --- hero ------------------------------------------------------------
    QString title;
    QString sub;
    Icons::Icon icon = Icons::Icon::Cube;
    IconTile::Tint tint = IconTile::Tint::Neutral;

    switch (state)
    {
    case InjectionSequence::State::WaitWindow:
        title = QStringLiteral("正在等待目标窗口");
        sub = QStringLiteral("启动标题包含关键字的 Minecraft 客户端后自动继续");
        icon = Icons::Icon::Gauge;
        tint = IconTile::Tint::Accent;
        break;
    case InjectionSequence::State::Injecting:
        title = QStringLiteral("正在注入代理 DLL");
        sub = QStringLiteral("正在向目标进程写入并启动反射加载器");
        icon = Icons::Icon::Inject;
        tint = IconTile::Tint::Accent;
        break;
    case InjectionSequence::State::WaitPort:
        title = QStringLiteral("等待代理端口就绪");
        sub = QStringLiteral("注入完成，正在等待 25565 端口开始监听（最长 30 秒）");
        icon = Icons::Icon::Clock;
        tint = IconTile::Tint::Accent;
        break;
    default:
        if (snapshot.observing && snapshot.alive && snapshot.listening)
        {
            title = QStringLiteral("代理运行中");
            const QStringList ips = AppPaths::lanIpAddresses();
            if (ips.isEmpty())
            {
                sub = QStringLiteral("B 客户端可通过 127.0.0.1:25565 加入 A 的会话");
            }
            else
            {
                sub = QStringLiteral("B 可通过 127.0.0.1:25565 或 %1:25565 加入 A 的会话")
                          .arg(ips.first());
            }
            icon = Icons::Icon::CheckCircle;
            tint = IconTile::Tint::Success;
        }
        else if (snapshot.observing && snapshot.alive && !snapshot.listening && snapshot.wasActive)
        {
            title = QStringLiteral("代理已停用");
            sub = QStringLiteral("在 A 窗口前台按 DEL 可停用；重新开始注入即可恢复");
            icon = Icons::Icon::AlertTriangle;
            tint = IconTile::Tint::Warning;
        }
        else if (snapshot.observing && !snapshot.alive)
        {
            title = QStringLiteral("A 客户端已退出");
            sub = QStringLiteral("目标进程已结束；重新启动游戏后再注入");
            icon = Icons::Icon::ErrorCircle;
            tint = IconTile::Tint::Neutral;
        }
        else if (snapshot.windowFound)
        {
            title = QStringLiteral("A 客户端已运行");
            sub = QStringLiteral("检测到目标窗口；前往「注入」页开始注入");
            icon = Icons::Icon::User;
            tint = IconTile::Tint::Neutral;
        }
        else
        {
            title = QStringLiteral("代理未运行");
            sub = QStringLiteral("点击「开始注入」启动一次注入流程；完成后 B 客户端可通过本机 25565 端口加入 A 的会话");
        }
        break;
    }

    m_heroTitle->setText(title);
    m_heroSub->setText(sub);
    m_heroIcon->setIcon(icon);
    m_heroIcon->setTint(tint);

    const bool working = (state == InjectionSequence::State::WaitWindow
                          || state == InjectionSequence::State::Injecting
                          || state == InjectionSequence::State::WaitPort);
    m_ctaPrimary->setVisible(!working);
    if (!working)
    {
        const bool suspended =
            snapshot.observing && snapshot.alive && !snapshot.listening && snapshot.wasActive;
        m_ctaPrimary->setText(suspended ? QStringLiteral("恢复代理") : QStringLiteral("开始注入"));
    }
    m_ctaSecondary->setVisible(true);

    // --- stat cards ----------------------------------------------------------
    if (snapshot.windowFound)
    {
        m_aValue->setText(QStringLiteral("已运行 · PID %1").arg(snapshot.windowPid));
        m_aCaption->setText(snapshot.windowTitle);
    }
    else
    {
        m_aValue->setText(QStringLiteral("未检测到"));
        m_aCaption->setText(QStringLiteral("等待标题包含关键字的客户端启动"));
    }

    if (snapshot.observing && snapshot.listening)
    {
        m_listenerValue->setText(QStringLiteral("127.0.0.1:25565"));
        m_listenerValue->setRole(StyledLabel::Role::Success);
        m_listenerCaption->setText(QStringLiteral("正在接受 B 客户端连接"));
    }
    else
    {
        m_listenerValue->setText(QStringLiteral("未监听"));
        m_listenerValue->setRole(StyledLabel::Role::Secondary);
        m_listenerCaption->setText(QStringLiteral("注入完成后自动开启"));
    }

    if (snapshot.observing && snapshot.listening && snapshot.bConnections > 0)
    {
        m_bValue->setText(QStringLiteral("已连接（%1 台）").arg(snapshot.bConnections));
        m_bValue->setRole(StyledLabel::Role::Success);
        m_bCaption->setText(QStringLiteral("B 正在共享 A 的会话"));
    }
    else
    {
        m_bValue->setText(QStringLiteral("未连接"));
        m_bValue->setRole(StyledLabel::Role::Secondary);
        m_bCaption->setText(QStringLiteral("B 在游戏内使用「直接连接」进入"));
    }
}
