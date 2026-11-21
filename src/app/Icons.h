#pragma once

#include <QColor>
#include <QRectF>

class QPainter;

/*
 * Hand-drawn 1.5 px linear icon set. Every glyph is a QPainter path defined
 * on a 24x24 design grid, so it stays crisp at any size and DPI, honours the
 * current theme colour, and needs no image assets.
 */
class Icons
{
public:
    enum class Icon
    {
        None,
        Gauge,          // 总览
        Inject,         // 注入 (tray + down arrow)
        Document,       // 日志
        Gear,           // 设置
        Info,           // 关于
        User,           // A 客户端
        Nodes,          // 监听
        Link,           // B 客户端
        Cube,           // 产品标识
        Folder,
        Play,
        Stop,
        Refresh,
        Check,
        CheckCircle,
        AlertTriangle,
        ErrorCircle,
        Clock,
        ExternalLink,
        Shield,
        Trash,
        Sun,
        Moon,
        Dot,            // filled status dot
        ChevronRight,
    };

    // Paints the icon fitted (aspect preserved, centered) into `rect`.
    static void paint(QPainter& painter, Icon icon, const QRectF& rect, const QColor& color,
                      double strokeWidth = 1.5);
};
