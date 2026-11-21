#pragma once

#include <QAbstractButton>
#include <QVariantAnimation>

/*
 * macOS-style toggle switch (44x26) with an animated knob, keyboard
 * activation via Space and a focus ring.
 */
class ToggleSwitch : public QAbstractButton
{
    Q_OBJECT

public:
    explicit ToggleSwitch(QWidget* parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void checkToggled();

private:
    QVariantAnimation m_knob; // 0 = off, 1 = on
    bool m_hover = false;
};
