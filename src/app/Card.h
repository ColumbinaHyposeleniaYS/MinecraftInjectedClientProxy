#pragma once

#include <QWidget>

class QVBoxLayout;

/*
 * Rounded card container (radius 12 px, hairline border). Content is added
 * through contentLayout().
 */
class Card : public QWidget
{
    Q_OBJECT

public:
    explicit Card(QWidget* parent = nullptr);

    void setPadding(int padding);
    void setSpacing(int spacing);
    QVBoxLayout* contentLayout();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVBoxLayout* m_layout = nullptr;
};
