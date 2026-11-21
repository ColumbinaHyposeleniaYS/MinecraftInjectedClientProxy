#pragma once

#include "app/Icons.h"

#include <QAbstractButton>
#include <QVariantAnimation>
#include <QWidget>

/*
 * macOS-flavoured controls, fully QPainter-drawn:
 *   - AppButton:  filled primary / neutral secondary / ghost / destructive ghost
 *   - IconButton: 32x32 square icon hit-target
 *
 * All states: hover, pressed, focus ring, disabled, plus an inline loading
 * spinner for AppButton.
 */
class AppButton : public QAbstractButton
{
    Q_OBJECT

public:
    enum class Style { Primary, Secondary, Ghost, GhostDanger };

    explicit AppButton(const QString& text, Style style, QWidget* parent = nullptr);

    void setIcon(Icons::Icon icon);
    void setLoading(bool loading);
    bool isLoading() const { return m_loading; }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QColor fillColor() const;
    QColor textColor() const;

    Style m_style;
    Icons::Icon m_icon = Icons::Icon::None;
    bool m_hover = false;
    bool m_loading = false;
    QVariantAnimation m_spinner; // drives the loading arc
};

class IconButton : public QAbstractButton
{
    Q_OBJECT

public:
    explicit IconButton(Icons::Icon icon, const QString& tooltip, QWidget* parent = nullptr);

    void setIcon(Icons::Icon icon);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    Icons::Icon m_icon;
    bool m_hover = false;
};
