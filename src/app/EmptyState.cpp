#include "app/EmptyState.h"

#include "app/Buttons.h"
#include "app/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>

EmptyState::EmptyState(Icons::Icon icon, const QString& title, const QString& subtitle,
                       QWidget* parent)
    : QWidget(parent), m_icon(icon), m_title(title), m_subtitle(subtitle)
{
    setMinimumHeight(280);
}

void EmptyState::setAction(AppButton* button)
{
    m_action = button;
    m_action->setParent(this);
    m_action->adjustSize();
    m_action->show();
    update();
}

void EmptyState::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto& pal = Theme::pal();

    const int bubble = 64;
    const qreal centerY = height() / 2.0 - (m_action ? 56.0 : 26.0);
    const QPointF bubbleCenter(width() / 2.0, centerY);
    const QRectF bubbleRect(bubbleCenter.x() - bubble / 2.0, bubbleCenter.y() - bubble / 2.0,
                            bubble, bubble);

    p.setPen(Qt::NoPen);
    p.setBrush(pal.controlBg);
    p.drawRoundedRect(bubbleRect, bubble / 2.0, bubble / 2.0);

    Icons::paint(p, m_icon, bubbleRect.adjusted(18, 18, -18, -18), pal.textTertiary, 1.5);

    p.setFont(Theme::font(15, 600));
    p.setPen(pal.textPrimary);
    p.drawText(QRectF(0, bubbleRect.bottom() + 16, width(), 22),
               Qt::AlignHCenter | Qt::TextSingleLine, m_title);

    if (!m_subtitle.isEmpty())
    {
        p.setFont(Theme::font(12, 400));
        p.setPen(pal.textSecondary);
        const QRectF subRect(24, bubbleRect.bottom() + 44, width() - 48, 44);
        p.drawText(subRect, Qt::AlignHCenter | Qt::TextWordWrap, m_subtitle);
    }
}

void EmptyState::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_action)
    {
        m_action->adjustSize();
        const int y = int(height() / 2.0 - 56.0 + 32.0 + 44.0 + 28.0);
        m_action->move(int(width() / 2.0 - m_action->width() / 2.0), y);
    }
}
