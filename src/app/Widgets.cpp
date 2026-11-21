#include "app/Widgets.h"

#include "app/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPalette>

// ---------------------------------------------------------------------------
// StyledLabel

namespace {
QColor roleColor(StyledLabel::Role role)
{
    const auto& pal = Theme::pal();
    switch (role)
    {
    case StyledLabel::Role::Primary:
        return pal.textPrimary;
    case StyledLabel::Role::Secondary:
        return pal.textSecondary;
    case StyledLabel::Role::Tertiary:
        return pal.textTertiary;
    case StyledLabel::Role::Accent:
        return pal.accent;
    case StyledLabel::Role::Success:
        return pal.success;
    case StyledLabel::Role::Warning:
        return pal.warning;
    case StyledLabel::Role::Danger:
        return pal.danger;
    }
    return pal.textPrimary;
}
} // namespace

StyledLabel::StyledLabel(Role role, const QString& text, int pixelSize, int weight,
                         QWidget* parent)
    : QLabel(text, parent), m_role(role), m_pixelSize(pixelSize), m_weight(weight)
{
    refresh();
    connect(Theme::instance(), &Theme::changed, this, [this] { refresh(); });
}

void StyledLabel::setRole(Role role)
{
    if (m_role == role)
        return;
    m_role = role;
    refresh();
}

void StyledLabel::refresh()
{
    QPalette pal = palette();
    pal.setColor(QPalette::WindowText, roleColor(m_role));
    setPalette(pal);
    setFont(Theme::font(m_pixelSize, m_weight));
    update();
}

// ---------------------------------------------------------------------------
// IconTile

IconTile::IconTile(int size, int iconExtent, QWidget* parent)
    : QWidget(parent), m_size(size), m_iconExtent(iconExtent)
{
    setFixedSize(size, size);
}

void IconTile::setIcon(Icons::Icon icon)
{
    m_icon = icon;
    update();
}

void IconTile::setTint(Tint tint)
{
    m_tint = tint;
    update();
}

void IconTile::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const auto& pal = Theme::pal();

    QColor tint;
    QColor icon;
    switch (m_tint)
    {
    case Tint::Neutral:
        tint = pal.controlBg;
        icon = pal.textSecondary;
        break;
    case Tint::Accent:
        tint = pal.accentSoft;
        icon = pal.accent;
        break;
    case Tint::Success:
        tint = pal.successSoft;
        icon = pal.success;
        break;
    case Tint::Warning:
        tint = pal.warningSoft;
        icon = pal.warning;
        break;
    case Tint::Danger:
        tint = pal.dangerSoft;
        icon = pal.danger;
        break;
    }

    const qreal radius = m_size * 0.24;
    const QRectF rect = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(tint);
    p.drawRoundedRect(rect, radius, radius);

    const QRectF iconRect((m_size - m_iconExtent) / 2.0, (m_size - m_iconExtent) / 2.0,
                          m_iconExtent, m_iconExtent);
    Icons::paint(p, m_icon, iconRect, icon, 1.5);
}

// ---------------------------------------------------------------------------
// NumberChip

NumberChip::NumberChip(int number, QWidget* parent)
    : QWidget(parent), m_number(number)
{
    setFixedSize(22, 22);
}

void NumberChip::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const auto& pal = Theme::pal();

    p.setPen(Qt::NoPen);
    p.setBrush(pal.controlBg);
    p.drawEllipse(QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5));

    p.setFont(Theme::font(11, 600));
    p.setPen(pal.textSecondary);
    p.drawText(rect(), Qt::AlignCenter, QString::number(m_number));
}
