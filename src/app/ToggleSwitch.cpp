#include "app/ToggleSwitch.h"

#include "app/Theme.h"

#include <QEnterEvent>
#include <QPainter>
#include <QPainterPath>

namespace {
constexpr int kSwitchWidth = 44;
constexpr int kSwitchHeight = 26;
constexpr int kKnobSize = 22;
} // namespace

ToggleSwitch::ToggleSwitch(QWidget* parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFixedSize(kSwitchWidth, kSwitchHeight);
    setAttribute(Qt::WA_Hover, true);

    m_knob.setStartValue(0.0);
    m_knob.setEndValue(1.0);
    m_knob.setDuration(160);
    connect(&m_knob, &QVariantAnimation::valueChanged, this,
            qOverload<>(&QWidget::update));
    connect(this, &QAbstractButton::toggled, this, &ToggleSwitch::checkToggled);
}

QSize ToggleSwitch::sizeHint() const
{
    return QSize(kSwitchWidth, kSwitchHeight);
}

void ToggleSwitch::checkToggled()
{
    m_knob.setDirection(isChecked() ? QVariantAnimation::Forward : QVariantAnimation::Backward);
    m_knob.start();
}

void ToggleSwitch::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto& pal = Theme::pal();
    const double t = m_knob.currentValue().toDouble();

    // Track.
    QColor track;
    if (!isEnabled())
        track = pal.controlBg;
    else if (isChecked())
        track = isDown() ? pal.accentPressed : (m_hover ? pal.accentHover : pal.accent);
    else
        track = isDown() ? pal.controlBgActive : (m_hover ? pal.controlBgHover : pal.controlBgActive);

    const QRectF trackRect = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(track);
    p.drawRoundedRect(trackRect, kSwitchHeight / 2.0, kSwitchHeight / 2.0);

    if (hasFocus() && isEnabled())
    {
        QColor ring = pal.accent;
        ring.setAlpha(96);
        p.setPen(QPen(ring, 3.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(trackRect.adjusted(-1.5, -1.5, 1.5, 1.5), kSwitchHeight / 2.0 + 1.5,
                          kSwitchHeight / 2.0 + 1.5);
    }

    // Knob.
    const double margin = 2.0;
    const double x = margin + t * (kSwitchWidth - kKnobSize - 2 * margin);
    const QRectF knobRect(x, (kSwitchHeight - kKnobSize) / 2.0, kKnobSize, kKnobSize);

    QColor knobColor = Qt::white;
    if (!isEnabled())
        knobColor = pal.dark ? QColor(0x8E, 0x8E, 0x93) : QColor(0xE6, 0xE6, 0xE8);

    p.setBrush(knobColor);
    p.setPen(QPen(pal.dark ? QColor(0, 0, 0, 60) : QColor(0, 0, 0, 26), 1.0));
    p.drawEllipse(knobRect);
}

void ToggleSwitch::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event);
    m_hover = true;
    update();
}

void ToggleSwitch::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    m_hover = false;
    update();
}
