#include "core/LogTail.h"

#include "core/AppPaths.h"
#include "core/Settings.h"

#include <QFileInfo>

namespace {
constexpr int kPollMs = 500;
constexpr qint64 kInitialTailBytes = 64 * 1024; // only the last 64 KB on load
} // namespace

LogTail::LogTail(QObject* parent)
    : QObject(parent)
{
    m_timer.setInterval(kPollMs);
    connect(&m_timer, &QTimer::timeout, this, &LogTail::pollSlot);
    m_timer.start();
    // The initial pick happens in rescan(), called by MainWindow once the
    // receivers are connected; the poll timer picks it up otherwise.
}

void LogTail::pollSlot()
{
    if (!m_file.isOpen())
    {
        pickActive(); // the log file may appear later
        return;
    }
    QFileInfo info(m_active);
    if (!info.exists())
    {
        m_file.close();
        m_pos = 0;
        m_pending.clear();
        m_active.clear(); // avoid the guard below re-hitting on a closed file
        pickActive();     // fall back to the next candidate
        return;
    }

    const qint64 size = info.size();
    if (size < m_pos)
    {
        // Truncated: reload from the beginning.
        m_pos = 0;
        m_pending.clear();
        if (m_file.seek(0))
        {
            m_pending = m_file.readAll();
            m_pos = m_pending.size();
            const QStringList lines = splitPending();
            emit logReset(lines);
        }
        return;
    }
    if (m_file.seek(m_pos))
    {
        const QByteArray chunk = m_file.readAll();
        if (!chunk.isEmpty())
        {
            m_pos += chunk.size();
            m_pending.append(chunk);
            const QStringList lines = splitPending();
            if (!lines.isEmpty())
                emit linesAppended(lines);
        }
    }
}

void LogTail::rescan()
{
    m_file.close();
    m_pos = 0;
    m_pending.clear();
    m_active.clear(); // force re-pick even if the current file still exists
    pickActive();
}

void LogTail::pickActive()
{
    if (!m_active.isEmpty() && QFileInfo::exists(m_active))
        return; // current source still alive

    m_file.close();
    m_pos = 0;
    m_pending.clear();
    m_active.clear();

    const QString overridePath = Settings::instance()->logPathOverride();
    QStringList candidates;
    if (!overridePath.isEmpty())
        candidates << overridePath;
    candidates << AppPaths::logCandidates();

    for (const QString& candidate : candidates)
    {
        if (!QFileInfo::exists(candidate))
            continue;
        if (!m_file.open(QIODevice::ReadOnly))
            continue;

        m_active = candidate;

        // Load the last 64 KB. Only drop the first (likely partial) line when
        // we actually started mid-file; small files are read from the top.
        const qint64 size = m_file.size();
        m_pos = qMax<qint64>(0, size - kInitialTailBytes);
        const bool startedMidFile = (m_pos > 0);
        QStringList lines;
        if (m_file.seek(m_pos))
        {
            QByteArray chunk = m_file.readAll();
            m_pos += chunk.size();
            if (startedMidFile)
            {
                const int firstNewline = chunk.indexOf('\n');
                if (firstNewline < 0)
                    chunk.clear();
                else
                    chunk.remove(0, firstNewline + 1);
            }
            m_pending = chunk;
            lines = splitPending();
        }

        emit sourceChanged(m_active);
        emit logReset(lines);
        return;
    }

    if (!m_active.isEmpty())
    {
        m_active.clear();
        emit sourceChanged(QString());
    }
}

QStringList LogTail::splitPending()
{
    QStringList lines;
    int start = 0;
    while (true)
    {
        const int idx = m_pending.indexOf('\n', start);
        if (idx < 0)
            break;
        QByteArray raw = m_pending.mid(start, idx - start);
        if (raw.endsWith('\r'))
            raw.chop(1);
        lines.append(QString::fromUtf8(raw));
        start = idx + 1;
    }
    m_pending.remove(0, start);
    return lines;
}
