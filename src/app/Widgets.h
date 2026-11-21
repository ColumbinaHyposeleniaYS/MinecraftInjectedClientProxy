#pragma once

#include "app/Icons.h"

#include <QLabel>
#include <QWidget>

/*
 * Small shared building blocks used by the pages:
 *   - StyledLabel: theme-aware text with a color role
 *   - IconTile:    tinted rounded-square icon container
 *   - NumberChip:  circled step number for guides
 */
class StyledLabel : public QLabel
{
    Q_OBJECT

public:
    enum class Role { Primary, Secondary, Tertiary, Accent, Success, Warning, Danger };

    explicit StyledLabel(Role role, const QString& text, int pixelSize, int weight,
                         QWidget* parent = nullptr);

    void setRole(Role role);

private:
    void refresh();

    Role m_role;
    int m_pixelSize;
    int m_weight;
};

class IconTile : public QWidget
{
    Q_OBJECT

public:
    enum class Tint { Neutral, Accent, Success, Warning, Danger };

    explicit IconTile(int size, int iconExtent, QWidget* parent = nullptr);

    void setIcon(Icons::Icon icon);
    void setTint(Tint tint);

    QSize sizeHint() const override { return QSize(m_size, m_size); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int m_size;
    int m_iconExtent;
    Icons::Icon m_icon = Icons::Icon::Cube;
    Tint m_tint = Tint::Neutral;
};

class NumberChip : public QWidget
{
    Q_OBJECT

public:
    explicit NumberChip(int number, QWidget* parent = nullptr);

    QSize sizeHint() const override { return QSize(22, 22); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int m_number;
};
