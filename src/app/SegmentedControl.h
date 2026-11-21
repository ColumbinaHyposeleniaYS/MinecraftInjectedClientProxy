#pragma once

#include <QString>
#include <QVariantAnimation>
#include <QVector>
#include <QWidget>

/*
 * macOS-style segmented control with an animated sliding thumb.
 */
class SegmentedControl : public QWidget
{
    Q_OBJECT

public:
    explicit SegmentedControl(QWidget* parent = nullptr);

    void addItem(const QString& id, const QString& label);
    int count() const { return int(m_items.size()); }
    QString currentId() const;
    void setCurrentId(const QString& id);           // programmatic, no signal
    void setItemEnabled(const QString& id, bool enabled);

    QSize sizeHint() const override;

signals:
    void currentChanged(const QString& id);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    struct Item
    {
        QString id;
        QString label;
        qreal width = 0.0;
        bool enabled = true;
    };

    int itemAt(qreal x) const;
    void moveThumb(int index, bool animate);
    void relayout();

    QVector<Item> m_items;
    int m_current = -1;
    int m_hover = -1;
    qreal m_thumbX = 2.0;
    qreal m_thumbW = 0.0;

    QVariantAnimation m_anim; // interpolates thumbX
    qreal m_animFrom = 0.0;
    qreal m_animTo = 0.0;
};
