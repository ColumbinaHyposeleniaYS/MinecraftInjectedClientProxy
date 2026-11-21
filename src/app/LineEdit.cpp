#include "app/LineEdit.h"

#include "app/Buttons.h"
#include "app/Theme.h"

#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>

// ---------------------------------------------------------------------------
// LineEdit

LineEdit::LineEdit(QWidget* parent)
    : QLineEdit(parent)
{
    setFrame(false);
    setFixedHeight(32);
    setMinimumWidth(160);
    setFont(Theme::font(13, 400));
    setTextMargins(11, 0, 11, 0);
    setCursor(Qt::IBeamCursor);
    setFocusPolicy(Qt::StrongFocus);
}

QSize LineEdit::sizeHint() const
{
    return QSize(200, 32);
}

void LineEdit::paintEvent(QPaintEvent* event)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto& pal = Theme::pal();
    const QRectF rect = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    if (isEnabled())
    {
        p.setPen(Qt::NoPen);
        p.setBrush(pal.fieldBg);
        p.drawRoundedRect(rect, Theme::ControlRadius, Theme::ControlRadius);

        if (hasFocus())
        {
            QColor halo = pal.accent;
            halo.setAlpha(70);
            p.setPen(QPen(halo, 2.5));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(rect.adjusted(1.0, 1.0, -1.0, -1.0),
                              Theme::ControlRadius - 1.0, Theme::ControlRadius - 1.0);

            p.setPen(QPen(pal.accent, 1.4));
            p.drawRoundedRect(rect, Theme::ControlRadius, Theme::ControlRadius);
        }
        else
        {
            p.setPen(QPen(pal.fieldBorder, 1.0));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(rect, Theme::ControlRadius, Theme::ControlRadius);
        }
    }
    else
    {
        p.setPen(Qt::NoPen);
        p.setBrush(pal.controlBg);
        p.drawRoundedRect(rect, Theme::ControlRadius, Theme::ControlRadius);
    }

    QLineEdit::paintEvent(event);
}

void LineEdit::focusInEvent(QFocusEvent* event)
{
    QLineEdit::focusInEvent(event);
    update();
}

void LineEdit::focusOutEvent(QFocusEvent* event)
{
    QLineEdit::focusOutEvent(event);
    update();
}

// ---------------------------------------------------------------------------
// BrowseField

BrowseField::BrowseField(QWidget* parent)
    : QWidget(parent)
{
    m_edit = new LineEdit(this);
    m_browse = new IconButton(Icons::Icon::Folder, QStringLiteral("浏览"), this);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(m_edit, 1);
    layout->addWidget(m_browse, 0, Qt::AlignVCenter);

    connect(m_browse, &QAbstractButton::clicked, this, &BrowseField::browse);
    connect(m_edit, &QLineEdit::textEdited, this, &BrowseField::textEdited);
}

void BrowseField::setMode(Mode mode)
{
    m_mode = mode;
}

void BrowseField::setPlaceholder(const QString& text)
{
    m_edit->setPlaceholderText(text);
}

void BrowseField::setDialogTitle(const QString& title)
{
    m_dialogTitle = title;
}

QString BrowseField::text() const
{
    return m_edit->text();
}

void BrowseField::setText(const QString& text)
{
    m_edit->setText(text);
}

void BrowseField::browse()
{
    QString title = m_dialogTitle;
    if (title.isEmpty())
        title = QStringLiteral("选择文件");

    QString selected;
    switch (m_mode)
    {
    case Mode::DllFile:
        selected = QFileDialog::getOpenFileName(this, title, m_edit->text(),
                                                QStringLiteral("动态链接库 (*.dll);;所有文件 (*.*)"));
        break;
    case Mode::LogFile:
        selected = QFileDialog::getOpenFileName(this, title, m_edit->text(),
                                                QStringLiteral("日志文件 (*.log *.txt);;所有文件 (*.*)"));
        break;
    case Mode::AnyFile:
        selected = QFileDialog::getOpenFileName(this, title, m_edit->text());
        break;
    case Mode::Directory:
        selected = QFileDialog::getExistingDirectory(this, title, m_edit->text());
        break;
    }

    if (!selected.isEmpty())
    {
        m_edit->setText(QDir::toNativeSeparators(selected));
        emit textEdited(m_edit->text());
    }
}
