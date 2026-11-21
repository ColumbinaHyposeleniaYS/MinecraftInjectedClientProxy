#pragma once

#include <QByteArray>
#include <QFile>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

/*
 * Tails the proxy log. The active file is chosen from (in order) the
 * user override and the well-known candidate paths, mirroring the engine's
 * own fallback order. Lines are only emitted once they are complete
 * (newline-terminated); a partial trailing line is buffered until more data
 * arrives. If the file is truncated it is reloaded.
 */
class LogTail : public QObject
{
    Q_OBJECT

public:
    explicit LogTail(QObject* parent = nullptr);

    void rescan(); // re-pick the active file and reload

    QString activePath() const { return m_active; }
    bool hasSource() const { return !m_active.isEmpty(); }

signals:
    void sourceChanged(const QString& path);
    void linesAppended(const QStringList& lines);
    void logReset(const QStringList& lines); // full (re)load, e.g. truncation

private:
    void pickActive();
    void pollSlot();
    QStringList splitPending();

    QString m_active;
    QFile m_file;
    qint64 m_pos = 0;
    QByteArray m_pending; // bytes of a not-yet-terminated line
    QTimer m_timer;
};
