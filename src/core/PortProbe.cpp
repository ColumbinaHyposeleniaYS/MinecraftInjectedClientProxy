#include "core/PortProbe.h"

#include <iphlpapi.h>

namespace micp {
namespace PortProbe {

// The table stores ports in network byte order.
uint16_t HostPort(DWORD networkPort)
{
    return uint16_t(((networkPort & 0xFF) << 8) | ((networkPort >> 8) & 0xFF));
}

template <typename Predicate>
bool WalkTable(bool listenersOnly, DWORD pid, uint16_t port, Predicate&& predicate)
{
    const TCP_TABLE_CLASS tableId =
        listenersOnly ? TCP_TABLE_OWNER_PID_LISTENER : TCP_TABLE_OWNER_PID_ALL;
    ULONG size = 0;
    DWORD rc = GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, tableId, 0);
    if (rc != ERROR_INSUFFICIENT_BUFFER || size == 0)
        return false;

    std::vector<BYTE> buffer(size);
    auto* table = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());
    rc = GetExtendedTcpTable(table, &size, FALSE, AF_INET, tableId, 0);
    if (rc != NO_ERROR)
        return false;

    for (DWORD i = 0; i < table->dwNumEntries; ++i)
    {
        const MIB_TCPROW_OWNER_PID& row = table->table[i];
        if (row.dwOwningPid == pid && HostPort(row.dwLocalPort) == port)
        {
            if (predicate(row))
                return true;
        }
    }
    return false;
}

bool IsListening(DWORD pid, uint16_t port)
{
    return WalkTable(true, pid, port, [](const MIB_TCPROW_OWNER_PID&) { return true; });
}

bool IsPortBusyByOther(uint16_t port, DWORD exceptPid)
{
    const TCP_TABLE_CLASS tableId = TCP_TABLE_OWNER_PID_LISTENER;
    ULONG size = 0;
    DWORD rc = GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, tableId, 0);
    if (rc != ERROR_INSUFFICIENT_BUFFER || size == 0)
        return false;

    std::vector<BYTE> buffer(size);
    auto* table = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());
    if (GetExtendedTcpTable(table, &size, FALSE, AF_INET, tableId, 0) != NO_ERROR)
        return false;

    for (DWORD i = 0; i < table->dwNumEntries; ++i)
    {
        const MIB_TCPROW_OWNER_PID& row = table->table[i];
        if (HostPort(row.dwLocalPort) == port && row.dwOwningPid != exceptPid)
            return true;
    }
    return false;
}

int EstablishedCount(DWORD pid, uint16_t port)
{
    ULONG size = 0;
    DWORD rc = GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if (rc != ERROR_INSUFFICIENT_BUFFER || size == 0)
        return 0;

    std::vector<BYTE> buffer(size);
    auto* table = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());
    if (GetExtendedTcpTable(table, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) != NO_ERROR)
        return 0;

    int count = 0;
    for (DWORD i = 0; i < table->dwNumEntries; ++i)
    {
        const MIB_TCPROW_OWNER_PID& row = table->table[i];
        if (row.dwOwningPid == pid && HostPort(row.dwLocalPort) == port
            && row.dwState == MIB_TCP_STATE_ESTAB)
        {
            ++count;
        }
    }
    return count;
}

} // namespace PortProbe
} // namespace micp
