#pragma once

#include <QString>
#include <QTimer>
#include <QVector>
#include <QWidget>

/*
 * Vertical step list for the injection flow. Each step has a state glyph
 * (pending dot / spinner / check / error) with a connector line between
 * steps, a label and an optional detail line.
 */
class Stepper : public QWidget
{
    Q_OBJECT

public:
    enum class StepState { Pending, Active, Done, Error };

    explicit Stepper(QWidget* parent = nullptr);

    void setSteps(const QStringList& labels);
    void setState(int index, StepState state, const QString& detail = QString());
    void failActive(const QString& detail); // marks the first Active step as Error
    void resetStates();
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void advanceSpinner();

private:
    struct Row
    {
        QString label;
        QString detail;
        StepState state = StepState::Pending;
    };

    int rowHeight(const Row& row) const;

    QVector<Row> m_rows;
    QTimer m_spinner; // runs while any step is Active
    int m_angle = 0;
};
