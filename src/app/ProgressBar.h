#pragma once

#include <QWidget>

/*
 * Slim determinate progress bar (used for the 30 s port wait).
 */
class ProgressBar : public QWidget
{
    Q_OBJECT

public:
    explicit ProgressBar(QWidget* parent = nullptr);

    void setRange(int min, int max);
    void setValue(int value); // clamped to [min, max]
    int value() const { return m_value; }
    void reset();

    QSize sizeHint() const override { return QSize(200, 6); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int m_min = 0;
    int m_max = 100;
    int m_value = 0;
};
