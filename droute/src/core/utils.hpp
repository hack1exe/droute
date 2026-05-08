#pragma once

#include "pch.h"

namespace droute {

    bool IsLocalAddr(const sockaddr_in& addr);
    bool IsMulticast(const sockaddr_in& addr);
    bool IsUdpSocket(SOCKET s);
    bool IsSameAddr(const sockaddr_in& a, const sockaddr_in& b);

    bool WaitForWrite(SOCKET s, int timeoutMs);
    bool WaitForRead(SOCKET s, int timeoutMs);
    bool SetNonBlocking(SOCKET s, bool nonBlock);

    bool SendAll(SOCKET s, const void* data, int len);
    bool RecvAll(SOCKET s, void* buf, int len);

    std::string AddrToString(const sockaddr_in& addr);

}
