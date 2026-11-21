#include "app/Sidebar.h"

#include "app/Theme.h"

#include <QEnterEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

namespace {
constexpr int kItemHeight = 34;
constexpr int kItemRadius = 10; // capsule selection
constexpr int kSidebarLeftMargin = 16;

QColor dotColor(Sidebar::StatusDot dot)
{
    const auto& pal = Theme::pal();
    switch (dot)
    {
    case Sidebar::StatusDot::Working:
        return pal.accent;
    case Sidebar::StatusDot::Active:
        return pal.success;
    case Sidebar::StatusDot::Suspended:
        return pal.warning;
    case Sidebar::StatusDot::Error:
        return pal.danger;
    case Sidebar::StatusDot::Inactive:
    default:
        return pal.textTertiary;
    }
}
} // namespace

// ---------------------------------------------------------------------------
// SidebarItem

SidebarItem::SidebarItem(Icons::Icon icon, const QString& label, QWidget* parent)
    : QAbstractButton(parent), m_icon(icon)
{
    setText(label);
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFixedHeight(kItemHeight);
    setAttribute(Qt::WA_Hover, true);
    setAccessibleName(label);
}

QSize SidebarItem::sizeHint() const
{
    return QSize(200, kItemHeight);
}

void SidebarItem::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto& pal = Theme::pal();
    const QRectF rect = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QColor fill = Qt::transparent;
    QColor fg = pal.textPrimary;
    QColor iconColor = pal.textSecondary;

    if (isChecked())
    {
        QColor soft = pal.accent;
        soft.setAlpha(pal.dark ? 70 : 40);
        fill = soft;
        if (isDown())
        {
            QColor down = pal.accent;
            down.setAlpha(pal.dark ? 90 : 56);
            fill = down;
        }
        fg = pal.accent;
        iconColor = pal.accent;
    }
    else if (isDown())
    {
        fill = pal.controlBgActive;
    }
    else if (m_hover)
    {
        fill = pal.controlBg;
        iconColor = pal.textPrimary;
    }

    if (fill != Qt::transparent)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRoundedRect(rect, kItemRadius, kItemRadius);
    }

    if (hasFocus())
    {
        QColor ring = pal.accent;
        ring.setAlpha(80);
        p.setPen(QPen(ring, 2.5));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect.adjusted(-1, -1, 1, 1), kItemRadius + 1, kItemRadius + 1);
    }

    const int iconExtent = 18;
    const QRectF iconRect(13, (height() - iconExtent) / 2.0, iconExtent, iconExtent);
    Icons::paint(p, m_icon, iconRect, iconColor, 1.5);

    p.setFont(Theme::font(13, isChecked() ? 600 : 500));
    p.setPen(fg);
    p.drawText(QRectF(40, 0, width() - 44, height()), Qt::AlignVCenter | Qt::TextSingleLine,
               text());
}

void SidebarItem::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event);
    m_hover = true;
    update();
}

void SidebarItem::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    m_hover = false;
    update();
}

// ---------------------------------------------------------------------------
// SidebarFooter

SidebarFooter::SidebarFooter(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(26);
}

void SidebarFooter::setStatus(Sidebar::StatusDot dot, const QString& text)
{
    m_dot = dot;
    m_text = text;
    update();
}

void SidebarFooter::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const auto& pal = Theme::pal();

    const QRectF dotRect(1, height() / 2.0 - 3.5, 7, 7);
    p.setPen(Qt::NoPen);
    p.setBrush(dotColor(m_dot));
    p.drawEllipse(dotRect);

    p.setFont(Theme::font(12, 500));
    p.setPen(pal.textSecondary);
    p.drawText(QRectF(16, 0, width() - 16 - 40, height()),
               Qt::AlignVCenter | Qt::TextSingleLine, m_text);

    p.setFont(Theme::font(11, 400));
    p.setPen(pal.textTertiary);
    p.drawText(QRectF(width() - 44, 0, 44, height()),
               Qt::AlignVCenter | Qt::AlignRight | Qt::TextSingleLine,
               QStringLiteral("v") + Theme::AppVersion);
}

// ---------------------------------------------------------------------------
// Sidebar

Sidebar::Sidebar(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(Theme::SidebarWidth);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(kSidebarLeftMargin, Theme::TitleBarHeight + 16,
                                 kSidebarLeftMargin, 14);
    m_layout->setSpacing(2);

    m_layout->addSpacing(46); // room for the painted header (glyph + name)

    m_footer = new SidebarFooter(this);
    m_layout->addStretch(1);
    m_layout->addWidget(m_footer);
}

void Sidebar::addItem(const QString& id, Icons::Icon icon, const QString& label)
{
    auto* item = new SidebarItem(icon, label, this);
    m_ids.append(id);
    m_group->addButton(item, int(m_ids.size()) - 1);
    m_layout->insertWidget(m_layout->count() - 2, item); // before stretch+footer

    connect(item, &QAbstractButton::clicked, this, [this, id] { emit currentChanged(id); });
}

void Sidebar::setCurrentId(const QString& id)
{
    const int index = int(m_ids.indexOf(id));
    if (index < 0)
        return;
    auto* button = m_group->button(index);
    if (button)
        button->setChecked(true);
}

QString Sidebar::currentId() const
{
    const int index = m_group->checkedId();
    if (index < 0 || index >= m_ids.size())
        return QString();
    return m_ids.at(index);
}

void Sidebar::setStatus(StatusDot dot, const QString& text)
{
    m_footer->setStatus(dot, text);
}

void Sidebar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const auto& pal = Theme::pal();

    QColor bg = pal.sidebarBg;
    if (Theme::translucentWindow())
        bg = Theme::wash(pal.sidebarBg, 0.72);
    p.fillRect(rect(), bg);

    // Hairline separator on the right edge.
    p.fillRect(QRect(width() - 1, 0, 1, height()), pal.separator);

    // Product header: cube glyph tile + two-line name.
    const qreal tileY = Theme::TitleBarHeight + 16;
    const QRectF tileRect(kSidebarLeftMargin, tileY, 34, 34);
    p.setPen(Qt::NoPen);
    p.setBrush(pal.accentSoft);
    p.drawRoundedRect(tileRect, 8, 8);
    Icons::paint(p, Icons::Icon::Cube, tileRect.adjusted(8, 8, -8, -8), pal.accent, 1.5);

    const QFont nameFont = Theme::font(12, 600);
    p.setFont(nameFont);
    p.setPen(pal.textPrimary);
    p.drawText(QRectF(kSidebarLeftMargin + 44, tileY, width() - kSidebarLeftMargin - 44 - 8, 17),
               Qt::AlignVCenter | Qt::TextSingleLine, QStringLiteral("Minecraft Injected"));
    p.drawText(QRectF(kSidebarLeftMargin + 44, tileY + 17,
                      width() - kSidebarLeftMargin - 44 - 8, 17),
               Qt::AlignVCenter | Qt::TextSingleLine, QStringLiteral("Client Proxy"));
}
