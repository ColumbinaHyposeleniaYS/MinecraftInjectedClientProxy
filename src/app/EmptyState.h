#pragma once

#include "app/Icons.h"

#include <QWidget>

class AppButton;

/*
 * Empty state: tinted icon bubble, title, subtitle and an optional action
 * button, centered inside the widget.
 */
class EmptyState : public QWidget
{
    Q_OBJECT

public:
    explicit EmptyState(Icons::Icon icon, const QString& title, const QString& subtitle,
                        QWidget* parent = nullptr);

    void setAction(AppButton* button); // takes ownership

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    Icons::Icon m_icon;
    QString m_title;
    QString m_subtitle;
    AppButton* m_action = nullptr;
};
