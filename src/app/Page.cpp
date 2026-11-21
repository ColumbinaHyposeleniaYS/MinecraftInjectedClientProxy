#include "app/Page.h"

#include "app/Theme.h"

#include <QPalette>

Page::Page(const QString& title, const QString& subtitle, QWidget* parent)
    : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(24, Theme::TitleBarHeight + 16, 24, 24);
    outer->setSpacing(0);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setFocusPolicy(Qt::NoFocus);
    m_scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_host = new QWidget(m_scroll);
    m_host->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_content = new QVBoxLayout(m_host);
    m_content->setContentsMargins(0, 0, 0, 0);
    m_content->setSpacing(20);

    m_title = new QLabel(title, m_host);
    m_title->setFont(Theme::font(20, 700));
    m_title->setAccessibleName(title);

    m_subTitle = new QLabel(subtitle, m_host);
    m_subTitle->setFont(Theme::font(12, 400));
    m_subTitle->setWordWrap(false);

    const auto restyle = [this] {
        QPalette pal = m_subTitle->palette();
        pal.setColor(QPalette::WindowText, Theme::pal().textSecondary);
        m_subTitle->setPalette(pal);
        m_title->setFont(Theme::font(20, 700));
        m_subTitle->setFont(Theme::font(12, 400));
    };
    restyle();
    connect(Theme::instance(), &Theme::changed, this, restyle);

    m_content->addWidget(m_title);
    m_content->addWidget(m_subTitle);
    m_content->addSpacing(4);

    m_scroll->setWidget(m_host);
    outer->addWidget(m_scroll);
}
