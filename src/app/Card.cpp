#include "app/Card.h"

#include "app/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

Card::Card(QWidget* parent)
    : QWidget(parent)
{
    contentLayout(); // creates the layout with default padding
}

void Card::setPadding(int padding)
{
    m_layout->setContentsMargins(padding, padding, padding, padding);
}

void Card::setSpacing(int spacing)
{
    m_layout->setSpacing(spacing);
}

QVBoxLayout* Card::contentLayout()
{
    if (!m_layout)
    {
        m_layout = new QVBoxLayout(this);
        m_layout->setContentsMargins(20, 20, 20, 20);
        m_layout->setSpacing(12);
    }
    return m_layout;
}

void Card::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto& pal = Theme::pal();

    QColor fill = pal.card;
    if (Theme::translucentWindow())
        fill = Theme::wash(pal.card, 0.94);

    const QRectF rect = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawRoundedRect(rect, Theme::CardRadius, Theme::CardRadius);

    p.setPen(QPen(pal.border, 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(rect, Theme::CardRadius, Theme::CardRadius);
}
