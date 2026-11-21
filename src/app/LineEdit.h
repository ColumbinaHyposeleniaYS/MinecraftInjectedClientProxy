#pragma once

#include <QLineEdit>
#include <QWidget>

class IconButton;
class LineEdit;

/*
 * Text field with macOS-style focus halo, plus a variant with a trailing
 * folder button that opens the native file dialog.
 */
class LineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit LineEdit(QWidget* parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
};

class BrowseField : public QWidget
{
    Q_OBJECT

public:
    enum class Mode { DllFile, LogFile, AnyFile, Directory };

    explicit BrowseField(QWidget* parent = nullptr);

    void setMode(Mode mode);
    void setPlaceholder(const QString& text);
    void setDialogTitle(const QString& title);

    QString text() const;
    void setText(const QString& text);

signals:
    void textEdited(const QString& text);

private slots:
    void browse();

private:
    LineEdit* m_edit;
    IconButton* m_browse;
    Mode m_mode = Mode::AnyFile;
    QString m_dialogTitle;
};
