#include "core/PortProbe.h"

#include <iphlpapi.h>

namespace {

// The table stores ports in network byte order.
quint16 hostPort(DWORD networkPort)
{
    return quint16(((networkPort & 0xFF) << 8) | ((networkPort >> 8) & 0xFF));
}

// Shared implementation for the LISTENER and the ALL (any state) variants.
template <typename Predicate>
bool walkTable(bool listenersOnly, DWORD pid, quint16 port, Predicate&& predicate)
{
    const ULONG tableId = listenersOnly ? TCP_TABLE_OWNER_PID_LISTENER : TCP_TABLE_OWNER_PID_ALL;
    ULONG size = 0;
    DWORD rc = GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, tableId, 0);
    if (rc != ERROR_INSUFFICIENT_BUFFER || size == 0)
        return false;

    QByteArray buffer(int(size), Qt::Uninitialized);
    auto* table = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());
    rc = GetExtendedTcpTable(table, &size, FALSE, AF_INET, tableId, 0);
    if (rc != NO_ERROR)
        return false;

    bool result = false;
    for (DWORD i = 0; i < table->dwNumEntries; ++i)
    {
        const MIB_TCPROW_OWNER_PID& row = table->table[i];
        if (row.dwOwningPid == pid && hostPort(row.dwLocalPort) == port)
        {
            if (predicate(row))
            {
                result = true;
                break;
            }
        }
    }
    return result;
}

} // namespace

bool PortProbe::isListening(DWORD pid, quint16 port)
{
    return walkTable(
        true, pid, port, [](const MIB_TCPROW_OWNER_PID&) { return true; });
}

int PortProbe::establishedCount(DWORD pid, quint16 port)
{
    const ULONG tableId = TCP_TABLE_OWNER_PID_ALL;
    ULONG size = 0;
    DWORD rc = GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, tableId, 0);
    if (rc != ERROR_INSUFFICIENT_BUFFER || size == 0)
        return 0;

    QByteArray buffer(int(size), Qt::Uninitialized);
    auto* table = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());
    if (GetExtendedTcpTable(table, &size, FALSE, AF_INET, tableId, 0) != NO_ERROR)
        return 0;

    int count = 0;
    for (DWORD i = 0; i < table->dwNumEntries; ++i)
    {
        const MIB_TCPROW_OWNER_PID& row = table->table[i];
        if (row.dwOwningPid == pid && hostPort(row.dwLocalPort) == port
            && row.dwState == MIB_TCP_STATE_ESTAB)
        {
            ++count;
        }
    }
    return count;
}
