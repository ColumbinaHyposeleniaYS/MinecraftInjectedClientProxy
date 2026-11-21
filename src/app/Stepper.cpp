#include "app/Stepper.h"

#include "app/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QTimerEvent>

namespace {
constexpr int kGlyphSize = 22;
constexpr int kGlyphSpacing = 8;    // vertical gap between glyph rows
constexpr int kLabelGapFromGlyph = 14;
constexpr int kDetailExtra = 18;
} // namespace

Stepper::Stepper(QWidget* parent)
    : QWidget(parent)
{
    m_spinner.setInterval(40);
    connect(&m_spinner, &QTimer::timeout, this, &Stepper::advanceSpinner);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void Stepper::setSteps(const QStringList& labels)
{
    m_rows.clear();
    for (const QString& label : labels)
    {
        Row row;
        row.label = label;
        m_rows.append(row);
    }
    updateGeometry();
    update();
}

void Stepper::setState(int index, StepState state, const QString& detail)
{
    if (index < 0 || index >= m_rows.size())
        return;
    m_rows[index].state = state;
    m_rows[index].detail = detail;
    updateGeometry();
    update();

    bool anyActive = false;
    for (const Row& row : m_rows)
        anyActive = anyActive || row.state == StepState::Active;
    if (anyActive && !m_spinner.isActive())
        m_spinner.start();
    if (!anyActive && m_spinner.isActive())
        m_spinner.stop();
}

void Stepper::failActive(const QString& detail)
{
    for (int i = 0; i < m_rows.size(); ++i)
    {
        if (m_rows[i].state == StepState::Active)
        {
            setState(i, StepState::Error, detail);
            return;
        }
    }
}

void Stepper::resetStates()
{
    for (Row& row : m_rows)
    {
        row.state = StepState::Pending;
        row.detail.clear();
    }
    m_spinner.stop();
    updateGeometry();
    update();
}

int Stepper::rowHeight(const Row& row) const
{
    return kGlyphSize + (row.detail.isEmpty() ? 0 : kDetailExtra);
}

QSize Stepper::sizeHint() const
{
    int total = 0;
    for (int i = 0; i < m_rows.size(); ++i)
    {
        total += rowHeight(m_rows.at(i));
        if (i + 1 < m_rows.size())
            total += kGlyphSpacing;
    }
    return QSize(320, qMax(total, 1));
}

void Stepper::advanceSpinner()
{
    m_angle = (m_angle + 18) % 360;
    update();
}

void Stepper::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto& pal = Theme::pal();

    int y = 0;
    for (int i = 0; i < m_rows.size(); ++i)
    {
        const Row& row = m_rows.at(i);
        const int glyphX = 11;
        const QRectF glyphRect(glyphX, y, kGlyphSize, kGlyphSize);
        const qreal centerX = glyphRect.center().x();
        const qreal centerY = glyphRect.center().y();

        // Connector to the next step.
        if (i + 1 < m_rows.size())
        {
            const int nextY = y + rowHeight(row) + kGlyphSpacing;
            QColor connector = pal.separator;
            if (row.state == StepState::Done)
                connector = pal.success;
            p.setPen(QPen(connector, 2.0));
            p.setCapStyle(Qt::RoundCap);
            p.drawLine(QPointF(centerX, centerY + kGlyphSize / 2.0 + 3.0),
                       QPointF(centerX, nextY + kGlyphSize / 2.0 - 3.0));
        }

        // Glyph.
        switch (row.state)
        {
        case StepState::Pending:
        {
            p.setPen(QPen(pal.textTertiary, 1.5));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(glyphRect.adjusted(4, 4, -4, -4));
            break;
        }
        case StepState::Active:
        {
            QPen pen(pal.accent, 2.0);
            pen.setCapStyle(Qt::RoundCap);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            const int start = -m_angle * 16;
            p.drawArc(glyphRect.adjusted(2.5, 2.5, -2.5, -2.5), start, 100 * 16);
            p.setBrush(pal.accent);
            p.setPen(Qt::NoPen);
            p.drawEllipse(glyphRect.center(), 2.0, 2.0);
            break;
        }
        case StepState::Done:
        {
            p.setPen(Qt::NoPen);
            p.setBrush(pal.successSoft);
            p.drawEllipse(glyphRect);
            Icons::paint(p, Icons::Icon::Check, glyphRect.adjusted(4, 4, -4, -4), pal.success,
                         1.8);
            break;
        }
        case StepState::Error:
        {
            p.setPen(Qt::NoPen);
            p.setBrush(pal.dangerSoft);
            p.drawEllipse(glyphRect);
            Icons::paint(p, Icons::Icon::ErrorCircle, glyphRect.adjusted(3.5, 3.5, -3.5, -3.5),
                         pal.danger, 1.6);
            break;
        }
        }

        // Label + detail.
        const int textX = int(glyphRect.right()) + kLabelGapFromGlyph;
        const bool labelEmphasis = (row.state == StepState::Active || row.state == StepState::Done);

        p.setFont(Theme::font(13, labelEmphasis ? 600 : 400));
        p.setPen(labelEmphasis || row.state == StepState::Error ? pal.textPrimary
                                                                : pal.textSecondary);
        p.drawText(QRect(textX, y, width() - textX, kGlyphSize),
                   Qt::AlignVCenter | Qt::TextSingleLine, row.label);

        if (!row.detail.isEmpty())
        {
            p.setFont(Theme::font(12, 400));
            p.setPen(pal.textTertiary);
            p.drawText(QRect(textX, y + kGlyphSize, width() - textX, kDetailExtra),
                       Qt::AlignTop | Qt::TextSingleLine, row.detail);
        }

        y += rowHeight(row) + kGlyphSpacing;
    }
}
