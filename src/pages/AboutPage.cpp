#include "pages/AboutPage.h"

#include "app/Card.h"
#include "app/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

AboutPage::AboutPage(QWidget* parent)
    : Page(QStringLiteral("关于"), QStringLiteral("关于本程序与使用说明"), parent)
{
    auto* content = contentLayout();

    // --- identity block ---------------------------------------------------------
    auto* identity = new QWidget(this);
    auto* identityLayout = new QVBoxLayout(identity);
    identityLayout->setContentsMargins(0, 8, 0, 0);
    identityLayout->setSpacing(6);
    identityLayout->setAlignment(Qt::AlignHCenter);

    auto* tile = new IconTile(64, 36, identity);
    tile->setIcon(Icons::Icon::Cube);
    tile->setTint(IconTile::Tint::Accent);

    auto* name = new StyledLabel(StyledLabel::Role::Primary,
                                 QString::fromUtf8(Theme::AppName), 20, 700, identity);
    name->setAlignment(Qt::AlignHCenter);
    auto* version = new StyledLabel(
        StyledLabel::Role::Secondary,
        QStringLiteral("版本 %1 · Minecraft Java 1.20.1 · Windows x64").arg(Theme::AppVersion),
        12, 400, identity);
    version->setAlignment(Qt::AlignHCenter);

    identityLayout->addWidget(tile, 0, Qt::AlignHCenter);
    identityLayout->addSpacing(4);
    identityLayout->addWidget(name);
    identityLayout->addWidget(version);

    content->addWidget(identity);

    // --- description card ----------------------------------------------------------
    auto* descCard = new Card(this);
    auto* desc = new StyledLabel(
        StyledLabel::Role::Secondary,
        QStringLiteral("A 客户端保持与远端服务器的真实连接；代理注入后在 A 进程内开启本机 25565 监听。"
                       "B 客户端连接该端口即可进入 A 所在的服务器，共享 A 的世界、物品栏与位置。"
                       "支持 A 入服前与入服后两种注入方式，中途接入会先同步 A 当前所在的世界。"),
        13, 400, descCard);
    desc->setWordWrap(true);
    descCard->contentLayout()->addWidget(desc);

    auto* runtime = new StyledLabel(
        StyledLabel::Role::Tertiary,
        QStringLiteral("运行要求：Windows 10/11 x64 · Minecraft Java 1.20.1（A、B 版本一致）· 注入器与游戏同权限 · "
                       "使用 %1 构建，界面基于 Qt %2")
            .arg(QString::fromUtf8(Theme::CompilerTag))
            .arg(QString::fromLatin1(qVersion())),
        11, 400, descCard);
    runtime->setWordWrap(true);
    descCard->contentLayout()->addWidget(runtime);

    content->addWidget(descCard);

    // --- usage card ---------------------------------------------------------------
    auto* usageCard = new Card(this);
    auto* usageTitle =
        new StyledLabel(StyledLabel::Role::Primary, QStringLiteral("使用要点"), 15, 600, usageCard);
    usageCard->contentLayout()->addWidget(usageTitle);

    struct Point
    {
        const char* text;
    };
    const Point points[] = {
        {"在「注入」页选择代理 DLL（默认在程序目录探测 Aurora.dll），点击「开始注入」"},
        {"程序会等待标题包含关键字的 Java 窗口，完成反射注入并等待端口就绪（最长 30 秒）"},
        {"若代理已在 A 进程内驻留，再次注入会直接发送恢复信号，不会重复加载 DLL"},
        {"A 窗口在前台时按 DEL 停用代理；更换磁盘上的 DLL 后需重启游戏才会生效"},
        {"B 客户端使用「直接连接」输入 127.0.0.1:25565（同机）或 A 的局域网 IP:25565"},
    };
    for (const Point& point : points)
    {
        auto* label = new StyledLabel(StyledLabel::Role::Secondary, QString::fromUtf8(point.text),
                                      12, 400, usageCard);
        label->setWordWrap(true);
        usageCard->contentLayout()->addWidget(label);
    }

    content->addWidget(usageCard);

    // --- footer -----------------------------------------------------------------
    auto* footNote = new StyledLabel(
        StyledLabel::Role::Tertiary,
        QStringLiteral("仅供学习交流使用；请遵守所玩服务器的规则，合理使用本工具。"),
        11, 400, this);
    footNote->setAlignment(Qt::AlignHCenter);
    content->addWidget(footNote);

    content->addStretch(1);
}
