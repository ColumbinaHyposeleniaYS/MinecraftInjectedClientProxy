#pragma once
// TCP table probes: is the injected proxy listening, how many B clients are
// attached right now. Uses GetExtendedTcpTable so no sockets are opened.

#include "common.h"
#include <cstdint>

namespace micp::PortProbe {

bool IsListening(DWORD pid, uint16_t port);
int EstablishedCount(DWORD pid, uint16_t port);

} // namespace micp::PortProbe
