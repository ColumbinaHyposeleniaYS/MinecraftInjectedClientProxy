#include "app/SegmentedControl.h"

#include "app/Theme.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {
constexpr int kHeight = 32;
constexpr int kPadding = 2;
constexpr int kMinItemWidth = 72;
constexpr int kItemTextPadding = 26;

qreal itemWidthFor(const QString& label)
{
    const QFontMetrics fm(Theme::font(13, 400));
    return qMax<qreal>(kMinItemWidth, fm.horizontalAdvance(label) + kItemTextPadding);
}
} // namespace

SegmentedControl::SegmentedControl(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
    setFixedHeight(kHeight);

    m_anim.setDuration(190);
    connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        const qreal t = value.toReal();
        m_thumbX = m_animFrom + (m_animTo - m_animFrom) * t;
        update();
    });
}

void SegmentedControl::addItem(const QString& id, const QString& label)
{
    Item item;
    item.id = id;
    item.label = label;
    item.width = itemWidthFor(label);
    m_items.append(item);

    if (m_current < 0)
    {
        m_current = int(m_items.size()) - 1;
        moveThumb(m_current, false);
    }
    updateGeometry();
    update();
}

QString SegmentedControl::currentId() const
{
    if (m_current < 0 || m_current >= m_items.size())
        return QString();
    return m_items.at(m_current).id;
}

void SegmentedControl::setCurrentId(const QString& id)
{
    for (int i = 0; i < m_items.size(); ++i)
    {
        if (m_items.at(i).id == id)
        {
            if (m_items.at(i).enabled)
            {
                const bool changed = (i != m_current);
                m_current = i;
                moveThumb(i, true);
                update();
                if (changed)
                    emit currentChanged(id);
            }
            return;
        }
    }
}

void SegmentedControl::setItemEnabled(const QString& id, bool enabled)
{
    for (auto& item : m_items)
    {
        if (item.id == id)
        {
            item.enabled = enabled;
            if (!enabled && m_items.at(m_current).id == id)
            {
                // Fall back to the first enabled item.
                for (int i = 0; i < m_items.size(); ++i)
                {
                    if (m_items.at(i).enabled)
                    {
                        m_current = i;
                        moveThumb(i, true);
                        break;
                    }
                }
            }
            update();
            return;
        }
    }
}

QSize SegmentedControl::sizeHint() const
{
    qreal total = 2 * kPadding;
    for (const Item& item : m_items)
        total += item.width;
    return QSize(int(total), kHeight);
}

int SegmentedControl::itemAt(qreal x) const
{
    qreal cursor = kPadding;
    for (int i = 0; i < m_items.size(); ++i)
    {
        if (x >= cursor && x < cursor + m_items.at(i).width)
            return i;
        cursor += m_items.at(i).width;
    }
    return -1;
}

void SegmentedControl::moveThumb(int index, bool animate)
{
    if (index < 0 || index >= m_items.size())
        return;

    qreal x = kPadding;
    for (int i = 0; i < index; ++i)
        x += m_items.at(i).width;
    const qreal w = m_items.at(index).width;

    if (!animate)
    {
        m_anim.stop();
        m_thumbX = x;
        m_thumbW = w;
        update();
        return;
    }

    m_animFrom = m_thumbX;
    m_animTo = x;
    m_thumbW = w;
    m_anim.stop();
    m_anim.setStartValue(0.0);
    m_anim.setEndValue(1.0);
    m_anim.start();
}

void SegmentedControl::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto& pal = Theme::pal();

    // Container.
    const QRectF container = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(pal.controlBg);
    p.drawRoundedRect(container, kHeight / 2.0, kHeight / 2.0);

    // Thumb (clipped inside the container).
    p.save();
    QPainterPath clip;
    clip.addRoundedRect(container, kHeight / 2.0, kHeight / 2.0);
    p.setClipPath(clip);

    const qreal thumbY = kPadding;
    const qreal thumbH = kHeight - 2 * kPadding;
    QRectF thumbRect(m_thumbX, thumbY, m_thumbW, thumbH);
    p.setBrush(pal.segmentedThumb);
    p.setPen(QPen(pal.dark ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 18), 1.0));
    p.drawRoundedRect(thumbRect, (kHeight - 2 * kPadding) / 2.0, (kHeight - 2 * kPadding) / 2.0);
    p.restore();

    if (hasFocus())
    {
        QColor ring = pal.accent;
        ring.setAlpha(90);
        p.setPen(QPen(ring, 3.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(container.adjusted(-1.5, -1.5, 1.5, 1.5), kHeight / 2.0 + 1.5,
                          kHeight / 2.0 + 1.5);
    }

    // Labels.
    p.setFont(Theme::font(13, m_current >= 0 ? 600 : 400));
    qreal x = kPadding;
    for (int i = 0; i < m_items.size(); ++i)
    {
        const Item& item = m_items.at(i);
        QColor color;
        if (!item.enabled)
            color = pal.textTertiary;
        else if (i == m_current)
            color = pal.textPrimary;
        else if (i == m_hover && item.enabled)
            color = pal.textPrimary;
        else
            color = pal.textSecondary;

        p.setPen(color);
        p.drawText(QRectF(x, 0, item.width, kHeight), Qt::AlignCenter, item.label);
        x += item.width;
    }
}

void SegmentedControl::mousePressEvent(QMouseEvent* event)
{
    const int index = itemAt(event->position().x());
    if (index >= 0 && m_items.at(index).enabled && index != m_current)
    {
        m_current = index;
        moveThumb(index, true);
        update();
        emit currentChanged(m_items.at(index).id);
    }
    setFocus();
}

void SegmentedControl::mouseMoveEvent(QMouseEvent* event)
{
    const int index = itemAt(event->position().x());
    if (index != m_hover)
    {
        m_hover = index;
        update();
    }
}

void SegmentedControl::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    m_hover = -1;
    update();
}

void SegmentedControl::keyPressEvent(QKeyEvent* event)
{
    int next = m_current;
    if (event->key() == Qt::Key_Left)
    {
        do
        {
            --next;
        } while (next >= 0 && !m_items.at(next).enabled);
    }
    else if (event->key() == Qt::Key_Right)
    {
        do
        {
            ++next;
        } while (next < m_items.size() && !m_items.at(next).enabled);
    }
    else
    {
        QWidget::keyPressEvent(event);
        return;
    }

    if (next >= 0 && next < m_items.size() && next != m_current)
    {
        m_current = next;
        moveThumb(next, true);
        update();
        emit currentChanged(m_items.at(next).id);
    }
}

void SegmentedControl::focusOutEvent(QFocusEvent* event)
{
    Q_UNUSED(event);
    update(); // drop the focus ring
}
