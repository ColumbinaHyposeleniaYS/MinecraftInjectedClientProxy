#include "core/LogTail.h"

#include "core/AppPaths.h"

#include <cwchar>

namespace micp {
namespace {

constexpr unsigned long long kInitialTailBytes = 64 * 1024; // last 64 KB on load

bool ReadChunk(HANDLE file, unsigned long long pos, std::string& out)
{
    out.clear();
    LARGE_INTEGER seek;
    seek.QuadPart = LONGLONG(pos);
    if (!SetFilePointerEx(file, seek, nullptr, FILE_BEGIN))
        return false;

    char buffer[8192];
    for (;;)
    {
        DWORD got = 0;
        if (!ReadFile(file, buffer, sizeof(buffer), &got, nullptr) || got == 0)
            break;
        out.append(buffer, got);
        if (out.size() > 1024 * 1024)
            break; // safety valve (1 MB per poll is already absurd)
    }
    return !out.empty();
}

unsigned long long FileSizeOf(const std::wstring& path)
{
    WIN32_FILE_ATTRIBUTE_DATA info;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &info))
        return 0;
    ULARGE_INTEGER size;
    size.HighPart = info.nFileSizeHigh;
    size.LowPart = info.nFileSizeLow;
    return size.QuadPart;
}

std::vector<std::wstring> SplitPending(std::string& pending)
{
    std::vector<std::wstring> lines;
    size_t start = 0;
    for (;;)
    {
        const size_t idx = pending.find('\n', start);
        if (idx == std::string::npos)
            break;
        std::string raw = pending.substr(start, idx - start);
        if (!raw.empty() && raw.back() == '\r')
            raw.pop_back();
        // The engine writes UTF-8.
        const int wide = MultiByteToWideChar(CP_UTF8, 0, raw.c_str(), int(raw.size()), nullptr, 0);
        std::wstring line;
        if (wide > 0)
        {
            line.resize(size_t(wide));
            MultiByteToWideChar(CP_UTF8, 0, raw.c_str(), int(raw.size()), &line[0], wide);
        }
        lines.push_back(std::move(line));
        start = idx + 1;
    }
    pending.erase(0, start);
    return lines;
}

} // namespace

void LogTail::PickActive(const std::wstring& overridePath)
{
    if (m_file != nullptr && m_file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_file);
        m_file = nullptr;
    }
    m_pos = 0;
    m_pending.clear();
    m_active.clear();

    std::vector<std::wstring> candidates;
    if (!overridePath.empty())
        candidates.push_back(overridePath);
    const auto auto_ = AppPaths::LogCandidates();
    candidates.insert(candidates.end(), auto_.begin(), auto_.end());

    for (const auto& candidate : candidates)
    {
        if (!FileExistsW(candidate))
            continue;
        HANDLE file = CreateFileW(candidate.c_str(), GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            continue;

        m_file = file;
        m_active = candidate;

        // Load the last 64 KB. Only drop the first (likely partial) line when
        // we actually started mid-file; small files are read from the top.
        const unsigned long long size = FileSizeOf(candidate);
        m_pos = size > kInitialTailBytes ? size - kInitialTailBytes : 0;
        const bool startedMidFile = (m_pos > 0);
        std::string chunk;
        if (ReadChunk(m_file, m_pos, chunk))
        {
            m_pos += chunk.size();
            if (startedMidFile)
            {
                const size_t firstNewline = chunk.find('\n');
                if (firstNewline == std::string::npos)
                    chunk.clear();
                else
                    chunk.erase(0, firstNewline + 1);
            }
            m_pending = std::move(chunk);
        }
        return;
    }
}

void LogTail::Rescan(const std::wstring& overridePath)
{
    m_active.clear(); // force re-pick even if the current file still exists
    PickActive(overridePath);
}

LogTail::Result LogTail::Poll(const std::wstring& overridePath)
{
    Result result;

    if (m_file == nullptr || m_file == INVALID_HANDLE_VALUE)
    {
        PickActive(overridePath);
        if (m_file != nullptr && m_file != INVALID_HANDLE_VALUE)
        {
            result.reset = true;
            result.source = m_active;
            result.lines = SplitPending(m_pending);
        }
        return result;
    }

    if (!FileExistsW(m_active))
    {
        PickActive(overridePath);
        result.reset = true;
        result.source = m_active;
        result.lines = SplitPending(m_pending);
        return result;
    }

    const unsigned long long size = FileSizeOf(m_active);
    if (size < m_pos)
    {
        // Truncated: reload from the beginning.
        m_pos = 0;
        m_pending.clear();
        std::string chunk;
        if (ReadChunk(m_file, 0, chunk))
        {
            m_pos = chunk.size();
            m_pending = std::move(chunk);
        }
        result.reset = true;
        result.source = m_active;
        result.lines = SplitPending(m_pending);
        return result;
    }

    std::string chunk;
    if (m_pos < size && ReadChunk(m_file, m_pos, chunk))
    {
        m_pos += chunk.size();
        m_pending.append(chunk);
        result.lines = SplitPending(m_pending);
    }
    return result;
}

} // namespace micp
