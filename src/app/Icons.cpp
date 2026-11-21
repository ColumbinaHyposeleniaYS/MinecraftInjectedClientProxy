#include "app/Icons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QtMath>

namespace {

QPen makePen(const QColor& color, double width)
{
    QPen pen(color, width);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    return pen;
}

QLineF line(double x1, double y1, double x2, double y2)
{
    return QLineF(x1, y1, x2, y2);
}

QRectF box(double x, double y, double w, double h)
{
    return QRectF(x, y, w, h);
}

QRectF circle(double cx, double cy, double r)
{
    return QRectF(cx - r, cy - r, 2 * r, 2 * r);
}

} // namespace

void Icons::paint(QPainter& painter, Icon icon, const QRectF& rect, const QColor& color,
                  double strokeWidth)
{
    if (icon == Icon::None || !rect.isValid() || !color.isValid())
        return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    const double scale = qMin(rect.width(), rect.height()) / 24.0;
    const double offsetX = rect.left() + (rect.width() - 24.0 * scale) / 2.0;
    const double offsetY = rect.top() + (rect.height() - 24.0 * scale) / 2.0;
    painter.translate(offsetX, offsetY);
    painter.scale(scale, scale);

    const QPen pen = makePen(color, strokeWidth);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    switch (icon)
    {
    case Icon::None:
        break;

    case Icon::Gauge:
    {
        // Speedometer arc + needle.
        painter.drawArc(circle(12, 13.4, 7.6), 200 * 16, 250 * 16);
        painter.drawLine(line(12, 13.4, 16.6, 8.8));
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(circle(12, 13.4, 1.15));
        break;
    }

    case Icon::Inject:
    {
        // Tray with a down arrow: loading something into the game.
        QPainterPath tray;
        tray.moveTo(4.4, 14.4);
        tray.lineTo(4.4, 16.6);
        tray.quadTo(4.4, 19.6, 7.4, 19.6);
        tray.lineTo(16.6, 19.6);
        tray.quadTo(19.6, 19.6, 19.6, 16.6);
        tray.lineTo(19.6, 14.4);
        painter.drawPath(tray);

        painter.drawLine(line(12, 3.6, 12, 12.8));
        painter.drawLine(line(8.6, 9.4, 12, 12.8));
        painter.drawLine(line(15.4, 9.4, 12, 12.8));
        break;
    }

    case Icon::Document:
    {
        QPainterPath doc;
        doc.addRoundedRect(box(5, 3.4, 14, 17.2), 2.2, 2.2);
        painter.drawPath(doc);
        painter.drawLine(line(8.6, 9.2, 15.4, 9.2));
        painter.drawLine(line(8.6, 12.6, 15.4, 12.6));
        painter.drawLine(line(8.6, 16, 12.6, 16));
        break;
    }

    case Icon::Gear:
    {
        painter.drawEllipse(circle(12, 12.4, 6.2));
        painter.drawEllipse(circle(12, 12.4, 2.6));
        for (int i = 0; i < 8; ++i)
        {
            const double angle = qDegreesToRadians(double(i * 45));
            const double c = qCos(angle), s = qSin(angle);
            painter.drawLine(QLineF(12 + c * 6.2, 12.4 + s * 6.2, 12 + c * 8.4, 12.4 + s * 8.4));
        }
        break;
    }

    case Icon::Info:
    {
        painter.drawEllipse(circle(12, 12, 8));
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(circle(12, 8.4, 0.95));
        painter.setPen(pen);
        painter.drawLine(line(12, 10.8, 12, 16));
        break;
    }

    case Icon::User:
    {
        painter.drawEllipse(circle(12, 8.2, 3.3));
        QPainterPath body;
        body.moveTo(5.2, 19.4);
        body.quadTo(12, 14.6, 18.8, 19.4);
        painter.drawPath(body);
        break;
    }

    case Icon::Nodes:
    {
        // Three linked nodes.
        const QPointF a(6.2, 7.4), b(6.2, 16.6), c(17.8, 12);
        painter.drawLine(QLineF(a, b));
        painter.drawLine(QLineF(a, c));
        painter.drawLine(QLineF(b, c));
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(circle(a.x(), a.y(), 2.1));
        painter.drawEllipse(circle(b.x(), b.y(), 2.1));
        painter.drawEllipse(circle(c.x(), c.y(), 2.1));
        break;
    }

    case Icon::Link:
    {
        // Two interlocked chain links, rotated 45 degrees.
        painter.save();
        painter.translate(12, 12);
        painter.rotate(-45);
        painter.drawRoundedRect(box(-8.6, -2.6, 10.2, 5.2), 2.6, 2.6);
        painter.drawRoundedRect(box(-1.6, -2.6, 10.2, 5.2), 2.6, 2.6);
        painter.restore();
        break;
    }

    case Icon::Cube:
    {
        // Isometric cube: the product mark.
        const QPointF top(12, 4.6);
        const QPointF upperRight(19.6, 9.0);
        const QPointF lowerRight(19.6, 17.8);
        const QPointF bottom(12, 22.2);
        const QPointF lowerLeft(4.4, 17.8);
        const QPointF upperLeft(4.4, 9.0);
        const QPointF center(12, 13.4);

        QPainterPath hex;
        hex.moveTo(top);
        hex.lineTo(upperRight);
        hex.lineTo(lowerRight);
        hex.lineTo(bottom);
        hex.lineTo(lowerLeft);
        hex.lineTo(upperLeft);
        hex.closeSubpath();
        painter.drawPath(hex);

        painter.drawLine(QLineF(center, upperRight));
        painter.drawLine(QLineF(center, upperLeft));
        painter.drawLine(QLineF(center, bottom));
        break;
    }

    case Icon::Folder:
    {
        QPainterPath folder;
        folder.moveTo(3.6, 8.4);
        folder.quadTo(3.6, 6.2, 5.8, 6.2);
        folder.lineTo(9.4, 6.2);
        folder.lineTo(11.2, 8.2);
        folder.lineTo(18.2, 8.2);
        folder.quadTo(20.4, 8.2, 20.4, 10.4);
        folder.lineTo(20.4, 16.8);
        folder.quadTo(20.4, 19, 18.2, 19);
        folder.lineTo(5.8, 19);
        folder.quadTo(3.6, 19, 3.6, 16.8);
        folder.closeSubpath();
        painter.drawPath(folder);
        break;
    }

    case Icon::Play:
    {
        QPainterPath tri;
        tri.moveTo(8.8, 5.6);
        tri.lineTo(18.2, 12);
        tri.lineTo(8.8, 18.4);
        tri.closeSubpath();
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawPath(tri);
        break;
    }

    case Icon::Stop:
    {
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(box(6.8, 6.8, 10.4, 10.4), 2.0, 2.0);
        break;
    }

    case Icon::Refresh:
    {
        painter.drawArc(circle(12, 12, 7.4), 60 * 16, 300 * 16);
        painter.drawLine(line(15.7, 4.8, 18.2, 7.6));
        painter.drawLine(line(18.2, 7.6, 14.5, 8.9));
        break;
    }

    case Icon::Check:
    {
        painter.drawLine(line(6.8, 12.8, 10.3, 16.2));
        painter.drawLine(line(10.3, 16.2, 17.3, 8.6));
        break;
    }

    case Icon::CheckCircle:
    {
        painter.drawEllipse(circle(12, 12, 8));
        painter.drawLine(line(8.5, 12.4, 10.8, 14.7));
        painter.drawLine(line(10.8, 14.7, 15.6, 9.4));
        break;
    }

    case Icon::AlertTriangle:
    {
        QPainterPath tri;
        tri.moveTo(12, 4.4);
        tri.lineTo(20.3, 18.7);
        tri.quadTo(20.9, 19.7, 19.6, 19.7);
        tri.lineTo(4.4, 19.7);
        tri.quadTo(3.1, 19.7, 3.7, 18.7);
        tri.closeSubpath();
        painter.drawPath(tri);
        painter.drawLine(line(12, 9.8, 12, 14.2));
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(circle(12, 16.9, 0.95));
        break;
    }

    case Icon::ErrorCircle:
    {
        painter.drawEllipse(circle(12, 12, 8));
        painter.drawLine(line(9.1, 9.1, 14.9, 14.9));
        painter.drawLine(line(14.9, 9.1, 9.1, 14.9));
        break;
    }

    case Icon::Clock:
    {
        painter.drawEllipse(circle(12, 12, 8));
        painter.drawLine(line(12, 12, 12, 7.6));
        painter.drawLine(line(12, 12, 15.4, 13.8));
        break;
    }

    case Icon::ExternalLink:
    {
        QPainterPath frame;
        frame.moveTo(4.6, 10.4);
        frame.lineTo(4.6, 17.8);
        painter.drawPath(frame);
        painter.drawLine(line(4.6, 17.8, 19.4, 17.8));
        QPainterPath left;
        left.moveTo(19.4, 17.8);
        left.lineTo(19.4, 11.6);
        painter.drawPath(left);
        painter.drawLine(line(11.2, 12.6, 18.4, 5.4));
        painter.drawLine(line(13.6, 5.0, 18.8, 5.0));
        painter.drawLine(line(18.8, 5.0, 18.8, 10.2));
        break;
    }

    case Icon::Shield:
    {
        QPainterPath shield;
        shield.moveTo(12, 3.8);
        shield.lineTo(19.2, 6.6);
        shield.lineTo(19.2, 11.4);
        shield.quadTo(19.2, 16.8, 12, 20.2);
        shield.quadTo(4.8, 16.8, 4.8, 11.4);
        shield.lineTo(4.8, 6.6);
        shield.closeSubpath();
        painter.drawPath(shield);
        painter.drawLine(line(9.2, 12.0, 11.2, 14.0));
        painter.drawLine(line(11.2, 14.0, 15.0, 9.6));
        break;
    }

    case Icon::Trash:
    {
        painter.drawLine(line(5.4, 7.4, 18.6, 7.4));
        QPainterPath handle;
        handle.moveTo(9.8, 7.4);
        handle.lineTo(9.8, 5.7);
        handle.quadTo(9.8, 4.4, 11.1, 4.4);
        handle.lineTo(12.9, 4.4);
        handle.quadTo(14.2, 4.4, 14.2, 5.7);
        handle.lineTo(14.2, 7.4);
        painter.drawPath(handle);
        QPainterPath body;
        body.moveTo(6.9, 7.4);
        body.lineTo(6.9, 16.9);
        body.quadTo(6.9, 19.6, 9.6, 19.6);
        body.lineTo(14.4, 19.6);
        body.quadTo(17.1, 19.6, 17.1, 16.9);
        body.lineTo(17.1, 7.4);
        painter.drawPath(body);
        painter.drawLine(line(10.4, 10.8, 10.4, 16.2));
        painter.drawLine(line(13.6, 10.8, 13.6, 16.2));
        break;
    }

    case Icon::Sun:
    {
        painter.drawEllipse(circle(12, 12, 3.7));
        for (int i = 0; i < 8; ++i)
        {
            const double angle = qDegreesToRadians(double(i * 45));
            const double c = qCos(angle), s = qSin(angle);
            painter.drawLine(QLineF(12 + c * 5.8, 12 + s * 5.8, 12 + c * 7.8, 12 + s * 7.8));
        }
        break;
    }

    case Icon::Moon:
    {
        QPainterPath moon;
        moon.moveTo(13.6, 4.6);
        moon.quadTo(12.4, 6.0, 12.4, 8.2);
        moon.quadTo(12.4, 12.4, 16.6, 14.4);
        moon.quadTo(17.8, 15.0, 19.2, 15.0);
        moon.quadTo(17.4, 19.4, 12.6, 19.4);
        moon.quadTo(6.8, 19.4, 6.8, 12.6);
        moon.quadTo(6.8, 7.6, 11.0, 5.2);
        moon.quadTo(12.2, 4.4, 13.6, 4.6);
        moon.closeSubpath();
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawPath(moon);
        break;
    }

    case Icon::Dot:
    {
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(circle(12, 12, 4.6));
        break;
    }

    case Icon::ChevronRight:
    {
        painter.drawLine(line(9.8, 7.0, 15.0, 12.0));
        painter.drawLine(line(15.0, 12.0, 9.8, 17.0));
        break;
    }
    }

    painter.restore();
}
