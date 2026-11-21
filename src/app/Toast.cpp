#include "app/Toast.h"

#include "app/Theme.h"

#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

namespace {
constexpr int kToastHeight = 40;
constexpr int kToastMaxWidth = 460;
constexpr int kIconExtent = 18;
constexpr int kMaxVisible = 3;
} // namespace

// ---------------------------------------------------------------------------
// Toast

Toast::Toast(Type type, const QString& text, QWidget* parent)
    : QWidget(parent), m_type(type), m_text(text)
{
    setCursor(Qt::PointingHandCursor);

    const QFontMetrics fm(Theme::font(13, 500));
    const int textWidth =
        qMin(fm.horizontalAdvance(text) + kIconExtent + 8 + 28 + 24, kToastMaxWidth);
    setFixedSize(textWidth, kToastHeight);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 5);
    shadow->setColor(QColor(0, 0, 0, 70));
    setGraphicsEffect(shadow);

    m_anim.setStartValue(0.0);
    m_anim.setEndValue(1.0);
    m_anim.setDuration(190);
    connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        const qreal t = v.toReal();
        m_opacity = t;
        m_offset = -12.0 * (1.0 - t);
        update();
    });
}

void Toast::flyIn(int lifeMs)
{
    m_anim.setDirection(QVariantAnimation::Forward);
    m_anim.start();

    m_life = new QTimer(this);
    m_life->setSingleShot(true);
    connect(m_life, &QTimer::timeout, this, &Toast::dismiss);
    m_life->start(lifeMs > 0 ? lifeMs
                             : (m_type == Type::Error ? 4200 : 3000));
}

void Toast::dismiss()
{
    if (m_dismissing)
        return;
    m_dismissing = true;

    connect(&m_anim, &QVariantAnimation::finished, this, [this] { deleteLater(); });
    m_anim.setDirection(QVariantAnimation::Backward);
    m_anim.start();
}

void Toast::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setOpacity(m_opacity);
    p.translate(0, m_offset);

    const auto& pal = Theme::pal();
    const QRectF rect = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    p.setPen(Qt::NoPen);
    p.setBrush(pal.toastBg);
    p.drawRoundedRect(rect, kToastHeight / 2.0, kToastHeight / 2.0);

    QColor iconColor = pal.accent;
    Icons::Icon icon = Icons::Icon::Info;
    switch (m_type)
    {
    case Type::Info:
        iconColor = pal.accent;
        icon = Icons::Icon::Info;
        break;
    case Type::Success:
        iconColor = pal.success;
        icon = Icons::Icon::CheckCircle;
        break;
    case Type::Error:
        iconColor = pal.danger;
        icon = Icons::Icon::ErrorCircle;
        break;
    }

    const int iconY = (height() - kIconExtent) / 2;
    Icons::paint(p, icon, QRectF(14, iconY, kIconExtent, kIconExtent), iconColor, 1.5);

    const int textX = 14 + kIconExtent + 8;
    const int textWidth = width() - textX - 14;
    const QFont font = Theme::font(13, 500);
    p.setFont(font);
    p.setPen(pal.toastText);
    p.drawText(QRectF(textX, 0, textWidth, height()), Qt::AlignVCenter | Qt::TextSingleLine,
               QFontMetrics(font).elidedText(m_text, Qt::ElideRight, textWidth));
}

void Toast::mousePressEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    dismiss();
}

// ---------------------------------------------------------------------------
// ToastHost

ToastHost::ToastHost(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents); // children stay interactive
    setAttribute(Qt::WA_NoSystemBackground);
}

void ToastHost::prune()
{
    for (int i = m_toasts.size() - 1; i >= 0; --i)
    {
        if (m_toasts.at(i).isNull())
            m_toasts.removeAt(i);
    }
}

void ToastHost::showToast(Toast::Type type, const QString& text, int ms)
{
    prune();
    while (m_toasts.size() >= kMaxVisible)
    {
        Toast* oldest = m_toasts.takeFirst();
        if (oldest)
            oldest->dismiss();
    }

    auto* toast = new Toast(type, text, this);
    m_toasts.append(toast);
    toast->show();
    layoutToasts();
    toast->raise();
    toast->flyIn(ms);
}

void ToastHost::layoutToasts()
{
    int y = Theme::TitleBarHeight + 8;
    for (int i = m_toasts.size() - 1; i >= 0; --i)
    {
        Toast* toast = m_toasts.at(i).data();
        if (toast && toast->isVisible())
        {
            toast->move(int(width() / 2.0 - toast->width() / 2.0), y);
            y += toast->height() + 8;
        }
    }
}

void ToastHost::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutToasts();
}
