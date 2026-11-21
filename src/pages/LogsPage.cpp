#include "pages/LogsPage.h"

#include "app/Buttons.h"
#include "app/Card.h"
#include "app/EmptyState.h"
#include "app/SegmentedControl.h"
#include "app/Theme.h"
#include "app/ToggleSwitch.h"
#include "core/Settings.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QString colorSpan(const QColor& color)
{
    return QStringLiteral("<span style=\"color:%1;\">").arg(color.name());
}

// Leading timestamp (e.g. [12-06 20:15:32] / 20:15:32) is dimmed.
QString dimTimestamp(const QString& escaped, const QString& dimColor)
{
    static const QRegularExpression re(
        QStringLiteral("(\\[?\\d{1,2}[:-]\\d{2}[:-]\\d{2}[ :-]\\d{2}(:\\d{2}(:\\d{2})?)?\\]?)"));
    const QRegularExpressionMatch match = re.match(escaped);
    if (match.hasMatch() && match.capturedStart(1) == 0)
    {
        return colorSpan(dimColor) + match.captured(1) + QStringLiteral("</span>")
               + escaped.mid(match.capturedLength(1));
    }
    return escaped;
}

} // namespace

// ---------------------------------------------------------------------------

LogsPage::LogsPage(QWidget* parent)
    : Page(QStringLiteral("日志"), QStringLiteral("实时查看代理运行日志，按严重级别过滤"), parent)
{
    auto* content = contentLayout();

    // --- toolbar ---------------------------------------------------------------
    auto* toolbar = new QWidget(this);
    auto* tools = new QHBoxLayout(toolbar);
    tools->setContentsMargins(0, 0, 0, 0);
    tools->setSpacing(12);

    m_filter = new SegmentedControl(toolbar);
    m_filter->addItem(QStringLiteral("all"), QStringLiteral("全部"));
    m_filter->addItem(QStringLiteral("warn"), QStringLiteral("警告及以上"));
    m_filter->addItem(QStringLiteral("error"), QStringLiteral("仅错误"));
    m_filter->setCurrentId(QStringLiteral("all"));

    auto* followLabel = new StyledLabel(StyledLabel::Role::Secondary, QStringLiteral("跟随滚动"), 12,
                                        400, toolbar);
    m_follow = new ToggleSwitch(toolbar);
    m_follow->setChecked(Settings::instance()->followLog());

    auto* refresh = new IconButton(Icons::Icon::Refresh, QStringLiteral("重新加载"), toolbar);
    auto* openDir = new IconButton(Icons::Icon::Folder, QStringLiteral("打开日志所在目录"), toolbar);
    auto* clear = new IconButton(Icons::Icon::Trash, QStringLiteral("清空显示"), toolbar);

    tools->addWidget(m_filter, 0, Qt::AlignVCenter);
    tools->addStretch(1);
    tools->addWidget(followLabel, 0, Qt::AlignVCenter);
    tools->addWidget(m_follow, 0, Qt::AlignVCenter);
    tools->addWidget(refresh, 0, Qt::AlignVCenter);
    tools->addWidget(openDir, 0, Qt::AlignVCenter);
    tools->addWidget(clear, 0, Qt::AlignVCenter);

    content->addWidget(toolbar);

    // --- source caption ----------------------------------------------------------
    m_sourceLabel = new StyledLabel(StyledLabel::Role::Tertiary,
                                    QStringLiteral("正在探测日志文件…"), 11, 400, this);
    content->addWidget(m_sourceLabel);

    // --- log view card -------------------------------------------------------------
    m_viewCard = new Card(this);
    m_viewCard->setPadding(6);
    m_view = new QTextEdit(m_viewCard);
    m_view->setReadOnly(true);
    m_view->setFont(Theme::font(12, 400));
    m_view->setLineWrapMode(QTextEdit::NoWrap);
    m_view->setFrameStyle(QFrame::NoFrame);
    m_viewCard->contentLayout()->addWidget(m_view);
    content->addWidget(m_viewCard, 1);

    // --- empty state ------------------------------------------------------------
    m_empty = new EmptyState(Icons::Icon::Document, QStringLiteral("暂无日志"),
                             QStringLiteral("开始注入后，代理日志会实时显示在这里；也可以在「设置」中手动指定日志文件"),
                             this);
    content->addWidget(m_empty, 1);
    m_empty->hide();

    content->addStretch(0);

    // --- wiring ----------------------------------------------------------------
    connect(m_filter, &SegmentedControl::currentChanged, this, [this](const QString& id) {
        if (id == QStringLiteral("warn"))
            m_filterLevel = 1;
        else if (id == QStringLiteral("error"))
            m_filterLevel = 2;
        else
            m_filterLevel = 0;
        renderAll();
    });
    connect(m_follow, &QAbstractButton::toggled, this, [this](bool on) {
        m_followOn = on;
        Settings::instance()->setFollowLog(on);
        if (on)
            scrollToBottom();
    });
    connect(refresh, &QAbstractButton::clicked, this, &LogsPage::rescanRequested);
    connect(clear, &QAbstractButton::clicked, this, [this] {
        m_entries.clear();
        m_view->clear();
        updateEmptyState();
    });
    connect(openDir, &QAbstractButton::clicked, this, [this] {
        const QString path = m_sourcePath;
        if (!path.isEmpty())
        {
            const QString dir = QDir::toNativeSeparators(QFileInfo(path).absolutePath());
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        }
    });

    reloadFromSettings();
    updateEmptyState();
}

void LogsPage::updateEmptyState()
{
    const bool empty = m_entries.isEmpty();
    m_empty->setVisible(empty);
    m_viewCard->setVisible(!empty);
}

void LogsPage::setSourcePath(const QString& path)
{
    m_sourcePath = path;
    if (path.isEmpty())
    {
        m_sourceLabel->setText(
            QStringLiteral("未找到日志文件；将在开始注入后自动探测（.minecraft\\proxy.log → TEMP → C:\\）"));
    }
    else
    {
        m_sourceLabel->setText(QStringLiteral("日志来源：%1").arg(QDir::toNativeSeparators(path)));
    }
}

void LogsPage::setFollowEnabled(bool on)
{
    m_follow->setChecked(on);
    m_followOn = on;
}

void LogsPage::reloadFromSettings()
{
    m_follow->setChecked(Settings::instance()->followLog());
}

int LogsPage::lineLevel(const QString& line) const
{
    if (line.contains(QLatin1String("ERROR"), Qt::CaseInsensitive)
        || line.contains(QLatin1String("[-]"))
        || line.contains(QStringLiteral("失败"), Qt::CaseInsensitive))
        return 3;
    if (line.contains(QLatin1String("WARN"), Qt::CaseInsensitive)
        || line.contains(QStringLiteral("警告"), Qt::CaseInsensitive))
        return 2;
    if (line.contains(QLatin1String("[+]")))
        return 1;
    return 0;
}

QString LogsPage::formatLine(const QString& line) const
{
    const auto& pal = Theme::pal();
    const QString escaped = line.toHtmlEscaped();

    QString color;
    switch (lineLevel(line))
    {
    case 3:
        color = pal.danger.name();
        break;
    case 2:
        color = pal.warning.name();
        break;
    case 1:
        color = pal.success.name();
        break;
    default:
        color = Theme::pal().dark ? QStringLiteral("#c8c8cc") : QStringLiteral("#3a3a3e");
        break;
    }

    return dimTimestamp(escaped, pal.textTertiary.name())
        .prepend(colorSpan(color) + QStringLiteral("&nbsp;"))
        .append(QStringLiteral("</span>"));
}

void LogsPage::appendHtmlLine(const QString& html)
{
    m_view->append(html);
}

bool LogsPage::atBottom() const
{
    const QScrollBar* bar = m_view->verticalScrollBar();
    return bar->maximum() - bar->value() <= 8;
}

void LogsPage::scrollToBottom()
{
    QScrollBar* bar = m_view->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void LogsPage::appendLines(const QStringList& lines)
{
    if (lines.isEmpty())
        return;

    const bool stick = m_followOn && atBottom();

    for (const QString& line : lines)
    {
        Entry entry;
        entry.raw = line;
        entry.level = lineLevel(line);
        m_entries.append(entry);
    }
    while (m_entries.size() > kMaxEntries)
        m_entries.removeFirst();

    for (const QString& line : lines)
    {
        const int level = lineLevel(line);
        const bool passes = (m_filterLevel == 0) || (m_filterLevel == 1 && level >= 2)
                            || (m_filterLevel == 2 && level >= 3);
        if (passes)
            appendHtmlLine(formatLine(line));
    }

    if (stick)
        scrollToBottom();
    updateEmptyState();
}

void LogsPage::resetLines(const QStringList& lines)
{
    m_entries.clear();
    for (const QString& line : lines)
    {
        Entry entry;
        entry.raw = line;
        entry.level = lineLevel(line);
        m_entries.append(entry);
    }
    while (m_entries.size() > kMaxEntries)
        m_entries.removeFirst();
    renderAll();
}

void LogsPage::renderAll()
{
    m_view->clear();
    for (const Entry& entry : m_entries)
    {
        const bool passes = (m_filterLevel == 0) || (m_filterLevel == 1 && entry.level >= 2)
                            || (m_filterLevel == 2 && entry.level >= 3);
        if (passes)
            appendHtmlLine(formatLine(entry.raw));
    }
    if (m_followOn)
        scrollToBottom();
    updateEmptyState();
}
