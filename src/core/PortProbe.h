#pragma once
// TCP table probes: is the injected proxy listening, how many B clients are
// attached right now. Uses GetExtendedTcpTable so no sockets are opened.

#include "common.h"
#include <cstdint>

namespace micp::PortProbe {

bool IsListening(DWORD pid, uint16_t port);
int EstablishedCount(DWORD pid, uint16_t port);

// True when some process OTHER than `exceptPid` is listening on `port`.
// Used before injection to warn early that the proxy bind will collide.
bool IsPortBusyByOther(uint16_t port, DWORD exceptPid);

} // namespace micp::PortProbe
