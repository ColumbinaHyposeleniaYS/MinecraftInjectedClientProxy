#pragma once

#include <QtGlobal>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>

/*
 * Passive probing of the TCP table: is the injected proxy of a given process
 * listening on its port, and how many established (B client) connections
 * does it currently own?
 */
class PortProbe
{
public:
    static bool isListening(DWORD pid, quint16 port);
    static int establishedCount(DWORD pid, quint16 port);
};
