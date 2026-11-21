#pragma once

#include <QAbstractButton>
#include <QList>
#include <QRect>
#include <QWidget>

/*
 * Self-drawn macOS-style traffic lights (close / minimize / maximize) used
 * purely as window controls, plus the title band that hosts them. The band
 * itself is transparent for mouse input: dragging, double-click maximize and
 * the right-click system menu are handled natively through WM_NCHITTEST in
 * MainWindow::nativeEvent, which carves out HTCLIENT regions over the
 * buttons so they still receive clicks.
 */
class TrafficLightButton : public QAbstractButton
{
    Q_OBJECT

public:
    enum class Kind { Close, Minimize, Maximize };

    explicit TrafficLightButton(Kind kind, QWidget* parent = nullptr);

    void setWindowActive(bool active);
    void setMaximized(bool maximized); // swaps maximize <-> restore glyph

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QColor baseColor() const;
    void drawGlyph(QPainter& p, const QPointF& center) const;

    Kind m_kind;
    bool m_active = true;
    bool m_hover = false;
    bool m_pressed = false;
    bool m_maximized = false;
};

class TitleBand : public QWidget
{
    Q_OBJECT

public:
    explicit TitleBand(QWidget* parent);

    void setTitleText(const QString& text);
    void setWindowActive(bool active);
    void setMaximized(bool maximized);

    // Button hit rects in window coordinates (the band sits at 0,0).
    QList<QRect> trafficHitRects() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    TrafficLightButton* m_close = nullptr;
    TrafficLightButton* m_minimize = nullptr;
    TrafficLightButton* m_maximize = nullptr;
    QString m_title;
    bool m_active = true;
};
