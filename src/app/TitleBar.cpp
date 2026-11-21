#include "app/TitleBar.h"

#include "app/Theme.h"

#include <QEnterEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {
constexpr int kButtonExtent = 16; // widget size (hit target)
constexpr int kCircleExtent = 12; // visible circle
constexpr int kGap = 8;           // gap between circle edges
constexpr int kStartX = 14;       // left margin to the first circle
} // namespace

// ---------------------------------------------------------------------------
// TrafficLightButton

TrafficLightButton::TrafficLightButton(Kind kind, QWidget* parent)
    : QAbstractButton(parent), m_kind(kind)
{
    setFixedSize(kButtonExtent, kButtonExtent);
    setCursor(Qt::ArrowCursor);
    setAttribute(Qt::WA_Hover, true);

    switch (kind)
    {
    case Kind::Close:
        setToolTip(QStringLiteral("关闭"));
        setAccessibleName(QStringLiteral("关闭窗口"));
        break;
    case Kind::Minimize:
        setToolTip(QStringLiteral("最小化"));
        setAccessibleName(QStringLiteral("最小化窗口"));
        break;
    case Kind::Maximize:
        setToolTip(QStringLiteral("最大化"));
        setAccessibleName(QStringLiteral("最大化或还原窗口"));
        break;
    }
}

void TrafficLightButton::setWindowActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    update();
}

void TrafficLightButton::setMaximized(bool maximized)
{
    if (m_maximized == maximized)
        return;
    m_maximized = maximized;
    if (m_kind == Kind::Maximize)
    {
        setToolTip(maximized ? QStringLiteral("向下还原") : QStringLiteral("最大化"));
        update();
    }
}

QSize TrafficLightButton::sizeHint() const
{
    return QSize(kButtonExtent, kButtonExtent);
}

QColor TrafficLightButton::baseColor() const
{
    const auto& pal = Theme::pal();
    if (!m_active)
        return pal.trafficInactive;
    switch (m_kind)
    {
    case Kind::Close:
        return pal.trafficRed;
    case Kind::Minimize:
        return pal.trafficYellow;
    case Kind::Maximize:
        return pal.trafficGreen;
    }
    return pal.trafficRed;
}

void TrafficLightButton::drawGlyph(QPainter& p, const QPointF& center) const
{
    const QColor glyph(0, 0, 0, 132);
    p.setPen(Qt::NoPen);
    p.setBrush(glyph);

    switch (m_kind)
    {
    case Kind::Close:
    {
        QPen pen(glyph, 1.2);
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(center.x() - 2.2, center.y() - 2.2),
                   QPointF(center.x() + 2.2, center.y() + 2.2));
        p.drawLine(QPointF(center.x() + 2.2, center.y() - 2.2),
                   QPointF(center.x() - 2.2, center.y() + 2.2));
        break;
    }
    case Kind::Minimize:
    {
        QPen pen(glyph, 1.2);
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(center.x() - 2.4, center.y()), QPointF(center.x() + 2.4, center.y()));
        break;
    }
    case Kind::Maximize:
    {
        // Two filled triangles pointing outward (or inward when maximized).
        QPainterPath top;
        QPainterPath bottom;
        if (!m_maximized)
        {
            // Outward: top triangle points up, bottom points down.
            top.moveTo(center.x(), center.y() - 2.7);
            top.lineTo(center.x() - 2.4, center.y() - 0.3);
            top.lineTo(center.x() + 2.4, center.y() - 0.3);
            top.closeSubpath();

            bottom.moveTo(center.x(), center.y() + 2.7);
            bottom.lineTo(center.x() - 2.4, center.y() + 0.3);
            bottom.lineTo(center.x() + 2.4, center.y() + 0.3);
            bottom.closeSubpath();
        }
        else
        {
            // Restore: triangles point toward each other.
            top.moveTo(center.x(), center.y() - 0.4);
            top.lineTo(center.x() - 2.4, center.y() - 2.8);
            top.lineTo(center.x() + 2.4, center.y() - 2.8);
            top.closeSubpath();

            bottom.moveTo(center.x(), center.y() + 0.4);
            bottom.lineTo(center.x() - 2.4, center.y() + 2.8);
            bottom.lineTo(center.x() + 2.4, center.y() + 2.8);
            bottom.closeSubpath();
        }
        p.drawPath(top);
        p.drawPath(bottom);
        break;
    }
    }
}

void TrafficLightButton::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QColor fill = baseColor();
    if (m_pressed)
    {
        fill = fill.darker(112);
    }
    else if (m_hover && m_active)
    {
        fill = fill.lighter(104);
    }

    const QPointF center((kButtonExtent - kCircleExtent) / 2.0 + kCircleExtent / 2.0,
                         (kButtonExtent - kCircleExtent) / 2.0 + kCircleExtent / 2.0);
    const QRectF rect(center.x() - kCircleExtent / 2.0, center.y() - kCircleExtent / 2.0,
                      kCircleExtent, kCircleExtent);

    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawEllipse(rect);

    if (m_hover && m_active)
        drawGlyph(p, center);
}

void TrafficLightButton::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event);
    m_hover = true;
    update();
}

void TrafficLightButton::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    m_hover = false;
    m_pressed = false;
    update();
}

void TrafficLightButton::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        m_pressed = true;
    QAbstractButton::mousePressEvent(event);
    update();
}

void TrafficLightButton::mouseReleaseEvent(QMouseEvent* event)
{
    QAbstractButton::mouseReleaseEvent(event);
    m_pressed = false;
    update();
}

// ---------------------------------------------------------------------------
// TitleBand

TitleBand::TitleBand(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFixedHeight(Theme::TitleBarHeight);

    m_close = new TrafficLightButton(TrafficLightButton::Kind::Close, this);
    m_minimize = new TrafficLightButton(TrafficLightButton::Kind::Minimize, this);
    m_maximize = new TrafficLightButton(TrafficLightButton::Kind::Maximize, this);

    connect(m_close, &QAbstractButton::clicked, this, [this] {
        if (QWidget* w = window())
            w->close();
    });
    connect(m_minimize, &QAbstractButton::clicked, this, [this] {
        if (QWidget* w = window())
            w->showMinimized();
    });
    connect(m_maximize, &QAbstractButton::clicked, this, [this] {
        if (QWidget* w = window())
        {
            if (w->isMaximized())
                w->showNormal();
            else
                w->showMaximized();
        }
    });
}

void TitleBand::setTitleText(const QString& text)
{
    m_title = text;
    update();
}

void TitleBand::setWindowActive(bool active)
{
    m_active = active;
    m_close->setWindowActive(active);
    m_minimize->setWindowActive(active);
    m_maximize->setWindowActive(active);
    update();
}

void TitleBand::setMaximized(bool maximized)
{
    m_maximize->setMaximized(maximized);
}

QList<QRect> TitleBand::trafficHitRects() const
{
    QList<QRect> rects;
    if (m_close)
        rects << m_close->geometry();
    if (m_minimize)
        rects << m_minimize->geometry();
    if (m_maximize)
        rects << m_maximize->geometry();
    return rects;
}

void TitleBand::paintEvent(QPaintEvent*)
{
    if (m_title.isEmpty() || width() < 540)
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const auto& pal = Theme::pal();

    p.setFont(Theme::font(13, 500));
    p.setPen(m_active ? pal.textSecondary : pal.textTertiary);
    p.drawText(QRectF(Theme::SidebarWidth, 0, width() - 2 * Theme::SidebarWidth,
                      Theme::TitleBarHeight),
               Qt::AlignCenter | Qt::TextSingleLine, m_title);
}

void TitleBand::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    const int y = int(Theme::TitleBarHeight / 2.0 - kButtonExtent / 2.0);
    const int step = kCircleExtent + kGap;
    const int x0 = int(kStartX + kCircleExtent / 2.0 - kButtonExtent / 2.0);
    m_close->move(x0, y);
    m_minimize->move(x0 + step, y);
    m_maximize->move(x0 + 2 * step, y);
}
