#include "app/ProgressBar.h"

#include "app/Theme.h"

#include <QPainter>
#include <QPainterPath>

ProgressBar::ProgressBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(6);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ProgressBar::setRange(int min, int max)
{
    m_min = min;
    m_max = qMax(min + 1, max);
    update();
}

void ProgressBar::setValue(int value)
{
    m_value = qBound(m_min, value, m_max);
    update();
}

void ProgressBar::reset()
{
    m_value = m_min;
    update();
}

void ProgressBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const auto& pal = Theme::pal();

    const QRectF track = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(pal.controlBg);
    p.drawRoundedRect(track, 3, 3);

    const double t = double(m_value - m_min) / double(m_max - m_min);
    if (t <= 0.0)
        return;
    const QRectF fill(0.5, 0.5, qMax(3.0, track.width() * t), track.height());
    p.setBrush(pal.accent);
    p.drawRoundedRect(fill, 3, 3);
}
