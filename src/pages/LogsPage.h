#pragma once

#include "app/Page.h"
#include "app/Widgets.h"

class EmptyState;
class IconButton;
class SegmentedControl;
class StyledLabel;
class ToggleSwitch;
class QTextEdit;

/*
 * 日志: follows the proxy log (auto-detected or overridden), with severity
 * filter, follow-tail toggle and open/refresh/clear actions.
 */
class LogsPage : public Page
{
    Q_OBJECT

public:
    explicit LogsPage(QWidget* parent = nullptr);

    void setSourcePath(const QString& path);
    void appendLines(const QStringList& lines);
    void resetLines(const QStringList& lines);
    void setFollowEnabled(bool on);
    void reloadFromSettings();

signals:
    void rescanRequested();

private:
    void renderAll();
    void updateEmptyState();
    void appendHtmlLine(const QString& html);
    QString formatLine(const QString& line) const;
    int lineLevel(const QString& line) const;
    bool atBottom() const;
    void scrollToBottom();

    QTextEdit* m_view = nullptr;
    SegmentedControl* m_filter = nullptr;
    ToggleSwitch* m_follow = nullptr;
    StyledLabel* m_sourceLabel = nullptr;
    EmptyState* m_empty = nullptr;
    QWidget* m_viewCard = nullptr;
    QString m_sourcePath;

    struct Entry
    {
        QString raw;
        int level = 0; // 0 info, 1 success, 2 warning, 3 error
    };
    QList<Entry> m_entries;
    int m_filterLevel = 0; // 0 all, 1 warnings+, 2 errors only
    bool m_followOn = true;
    static constexpr int kMaxEntries = 4000;
};
