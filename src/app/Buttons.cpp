#include "app/Buttons.h"

#include "app/Theme.h"

#include <QEnterEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

namespace {

constexpr int kButtonHeight = 34;
constexpr int kButtonRadius = Theme::ControlRadius;
constexpr int kIconExtent = 18;

void drawFocusRing(QPainter& p, const QRectF& rect, int radius)
{
    const QColor ring = Theme::pal().accent;
    QColor c = ring;
    c.setAlpha(96);
    QPen pen(c, 2.5);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const int r = qMax(1, radius - 1);
    p.drawRoundedRect(rect.adjusted(1.0, 1.0, -1.0, -1.0), r, r);
}

} // namespace

// ---------------------------------------------------------------------------
// AppButton

AppButton::AppButton(const QString& text, Style style, QWidget* parent)
    : QAbstractButton(parent), m_style(style)
{
    setText(text);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFixedHeight(kButtonHeight);
    setAttribute(Qt::WA_Hover, true);

    m_spinner.setStartValue(0);
    m_spinner.setEndValue(360);
    m_spinner.setDuration(900);
    m_spinner.setLoopCount(-1);
    connect(&m_spinner, &QVariantAnimation::valueChanged, this,
            qOverload<>(&QWidget::update));
}

void AppButton::setIcon(Icons::Icon icon)
{
    m_icon = icon;
    update();
}

void AppButton::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    if (m_loading)
        m_spinner.start();
    else
        m_spinner.stop();
    update();
}

QSize AppButton::sizeHint() const
{
    const QFontMetrics fm(Theme::font(13, 600));
    int textWidth = fm.horizontalAdvance(text());
    if (m_icon != Icons::Icon::None || m_loading)
        textWidth += kIconExtent + 8;
    return QSize(textWidth + 32, kButtonHeight);
}

QColor AppButton::fillColor() const
{
    const auto& p = Theme::pal();
    const bool disabled = !isEnabled();

    switch (m_style)
    {
    case Style::Primary:
        if (disabled)
            return p.controlBg;
        if (isDown())
            return p.accentPressed;
        if (m_hover)
            return p.accentHover;
        return p.accent;
    case Style::Secondary:
        if (isDown())
            return p.controlBgActive;
        if (m_hover)
            return p.controlBgHover;
        return p.controlBg;
    case Style::Ghost:
    case Style::GhostDanger:
        if (isDown())
            return p.controlBgActive;
        if (m_hover)
            return p.controlBg;
        return Qt::transparent;
    }
    return p.controlBg;
}

QColor AppButton::textColor() const
{
    const auto& p = Theme::pal();
    const bool disabled = !isEnabled();

    switch (m_style)
    {
    case Style::Primary:
        return disabled ? p.textTertiary : p.onAccent;
    case Style::Secondary:
        return disabled ? p.textTertiary : p.textPrimary;
    case Style::Ghost:
        return disabled ? p.textTertiary : (m_hover ? p.textPrimary : p.textSecondary);
    case Style::GhostDanger:
        return disabled ? p.textTertiary : p.danger;
    }
    return p.textPrimary;
}

void AppButton::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF rect = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QColor fill = fillColor();
    const QColor contentColor = textColor();

    if (fill != Qt::transparent)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRoundedRect(rect, kButtonRadius, kButtonRadius);
    }

    if (hasFocus() && isEnabled())
        drawFocusRing(p, rect, kButtonRadius);

    // Content.
    QFont font = Theme::font(13, 600);
    p.setFont(font);
    const QFontMetrics fm(font);

    const int textWidth = fm.horizontalAdvance(text());
    int contentWidth = textWidth;
    const bool hasIcon = m_icon != Icons::Icon::None || m_loading;
    if (hasIcon)
        contentWidth += kIconExtent + 8;
    int x = int((width() - contentWidth) / 2.0);

    if (hasIcon)
    {
        const int iconY = (height() - kIconExtent) / 2;
        if (m_loading)
        {
            // Spinner arc.
            QPen pen(contentColor);
            pen.setWidthF(1.8);
            pen.setCapStyle(Qt::RoundCap);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            const QRectF arcRect(x + 2, iconY + 2, kIconExtent - 4, kIconExtent - 4);
            const int start = -int(m_spinner.currentValue().toInt()) * 16;
            p.drawArc(arcRect, start, 100 * 16);
        }
        else
        {
            Icons::paint(p, m_icon, QRectF(x, iconY, kIconExtent, kIconExtent), contentColor,
                         1.5);
        }
        x += kIconExtent + 8;
    }

    p.setPen(contentColor);
    p.drawText(QRect(x, 0, textWidth + 4, height()), Qt::AlignVCenter | Qt::TextSingleLine,
               text());
}

void AppButton::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event);
    m_hover = true;
    update();
}

void AppButton::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    m_hover = false;
    update();
}

// ---------------------------------------------------------------------------
// IconButton

IconButton::IconButton(Icons::Icon icon, const QString& tooltip, QWidget* parent)
    : QAbstractButton(parent), m_icon(icon)
{
    setToolTip(tooltip);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFixedSize(32, 32);
    setAttribute(Qt::WA_Hover, true);
}

void IconButton::setIcon(Icons::Icon icon)
{
    m_icon = icon;
    update();
}

QSize IconButton::sizeHint() const
{
    return QSize(32, 32);
}

void IconButton::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto& pal = Theme::pal();
    QColor fill = Qt::transparent;
    if (!isEnabled())
        fill = Qt::transparent;
    else if (isDown())
        fill = pal.controlBgActive;
    else if (m_hover)
        fill = pal.controlBgHover;

    const QRectF rect = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    if (fill != Qt::transparent)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRoundedRect(rect, 8, 8);
    }

    if (hasFocus() && isEnabled())
        drawFocusRing(p, rect, 8);

    const QColor color = isEnabled() ? pal.textSecondary : pal.textTertiary;
    Icons::paint(p, m_icon, QRectF(7, 7, 18, 18), color, 1.5);
}

void IconButton::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event);
    m_hover = true;
    update();
}

void IconButton::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    m_hover = false;
    update();
}
