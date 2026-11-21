#pragma once

#include "app/Icons.h"

#include <QList>
#include <QPointer>
#include <QVariantAnimation>
#include <QWidget>

class QTimer;

/*
 * macOS-style notification capsules that float in from below the title bar,
 * stack up to three and auto-dismiss. Click anywhere on a toast to dismiss.
 */
class Toast : public QWidget
{
    Q_OBJECT

public:
    enum class Type { Info, Success, Error };

    Toast(Type type, const QString& text, QWidget* parent);

    void flyIn(int lifeMs = -1); // -1 = auto (3 s, errors 4.2 s)
    void dismiss(); // animated out, then deletes itself

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    Type m_type;
    QString m_text;
    qreal m_opacity = 0.0;
    qreal m_offset = -12.0;

    QVariantAnimation m_anim; // drives opacity/offset, both directions
    QTimer* m_life = nullptr;
    bool m_dismissing = false;
};

class ToastHost : public QWidget
{
    Q_OBJECT

public:
    explicit ToastHost(QWidget* parent);

    void showToast(Toast::Type type, const QString& text, int ms = -1);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void layoutToasts();
    void prune();

    QList<QPointer<Toast>> m_toasts;
};
