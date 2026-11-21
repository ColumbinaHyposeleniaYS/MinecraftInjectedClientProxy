#pragma once

#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

/*
 * Base class for every page: scrollable content column below the title bar,
 * with a page title and subtitle header. Content widgets go into
 * contentLayout().
 */
class Page : public QWidget
{
    Q_OBJECT

public:
    explicit Page(const QString& title, const QString& subtitle, QWidget* parent = nullptr);

    QVBoxLayout* contentLayout() const { return m_content; }
    QLabel* titleLabel() const { return m_title; }
    QLabel* subtitleLabel() const { return m_subTitle; }

private:
    QScrollArea* m_scroll = nullptr;
    QWidget* m_host = nullptr;
    QVBoxLayout* m_content = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_subTitle = nullptr;
};
