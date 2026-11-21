#pragma once

#include "app/Icons.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

class QLabel;

/*
 * 220 px frosted sidebar: product header, capsule-selection navigation and a
 * live proxy status footer.
 */
class SidebarItem : public QAbstractButton
{
    Q_OBJECT

public:
    SidebarItem(Icons::Icon icon, const QString& label, QWidget* parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    Icons::Icon m_icon;
    bool m_hover = false;
};

class SidebarFooter;

class Sidebar : public QWidget
{
    Q_OBJECT

public:
    enum class StatusDot { Inactive, Working, Active, Suspended, Error };

    explicit Sidebar(QWidget* parent = nullptr);

    void addItem(const QString& id, Icons::Icon icon, const QString& label);
    void setCurrentId(const QString& id);
    QString currentId() const;

    void setStatus(StatusDot dot, const QString& text);

signals:
    void currentChanged(const QString& id);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QStringList m_ids;
    QButtonGroup* m_group = nullptr;
    QVBoxLayout* m_layout = nullptr;
    SidebarFooter* m_footer = nullptr;
};

/*
 * Bottom line: colored status dot + label on the left, version on the right.
 */
class SidebarFooter : public QWidget
{
    Q_OBJECT

public:
    explicit SidebarFooter(QWidget* parent = nullptr);

    void setStatus(Sidebar::StatusDot dot, const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    Sidebar::StatusDot m_dot = Sidebar::StatusDot::Inactive;
    QString m_text;
};
