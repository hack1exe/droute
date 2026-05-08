#include "pch.h"
#include "src/core/utils.hpp"
#include "src/core/config.hpp"
#include "src/core/logger.hpp"

namespace droute {

    bool IsLocalAddr(const sockaddr_in& addr) {
        if (addr.sin_family != AF_INET) return false;
        uint32_t ip = ntohl(addr.sin_addr.s_addr);
        if (ip == 0x7F000001) return true;
        if ((ip & 0xFF000000) == 0x7F000000) return true;
        if ((ip & 0xFF000000) == 0x0A000000) return true;
        if ((ip & 0xFFF00000) == 0xAC100000) return true;
        if ((ip & 0xFFFF0000) == 0xC0A80000) return true;
        if ((ip & 0xFFFF0000) == 0xA9FE0000) return true;
        return false;
    }

    bool IsMulticast(const sockaddr_in& addr) {
        if (addr.sin_family != AF_INET) return false;
        uint32_t ip = ntohl(addr.sin_addr.s_addr);
        return (ip & 0xF0000000) == 0xE0000000;
    }

    bool IsUdpSocket(SOCKET s) {
        int type = 0;
        int len = sizeof(type);
        if (getsockopt(s, SOL_SOCKET, SO_TYPE, (char*)&type, &len) != 0)
            return false;
        return type == SOCK_DGRAM;
    }

    bool IsSameAddr(const sockaddr_in& a, const sockaddr_in& b) {
        return a.sin_family == b.sin_family &&
               a.sin_addr.s_addr == b.sin_addr.s_addr &&
               a.sin_port == b.sin_port;
    }

    bool WaitForSocket(SOCKET s, bool write, int timeoutMs) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(s, &fds);

        timeval tv;
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;

        int r = select(0, write ? nullptr : &fds, write ? &fds : nullptr, nullptr, &tv);
        return (r > 0 && FD_ISSET(s, &fds));
    }

    bool WaitForWrite(SOCKET s, int timeoutMs) {
        return WaitForSocket(s, true, timeoutMs);
    }

    bool WaitForRead(SOCKET s, int timeoutMs) {
        return WaitForSocket(s, false, timeoutMs);
    }

    bool SetNonBlocking(SOCKET s, bool nonBlock) {
        u_long mode = nonBlock ? 1 : 0;
        return ioctlsocket(s, FIONBIO, &mode) == NO_ERROR;
    }

    bool SendAll(SOCKET s, const void* data, int len) {
        const char* p = static_cast<const char*>(data);
        int sent = 0;
        while (sent < len) {
            int n = ::send(s, p + sent, len - sent, 0);
            if (n > 0) {
                sent += n;
            } else {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK) {
                    if (!WaitForWrite(s, g_cfg.connectTimeout))
                        return false;
                } else {
                    return false;
                }
            }
        }
        return true;
    }

    bool RecvAll(SOCKET s, void* buf, int len) {
        char* p = static_cast<char*>(buf);
        int recvd = 0;
        while (recvd < len) {
            int n = ::recv(s, p + recvd, len - recvd, 0);
            if (n > 0) {
                recvd += n;
            } else if (n == 0) {
                return false;
            } else {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK) {
                    if (!WaitForRead(s, g_cfg.connectTimeout))
                        return false;
                } else {
                    return false;
                }
            }
        }
        return true;
    }

    std::string AddrToString(const sockaddr_in& addr) {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
        char buf[64];
        snprintf(buf, sizeof(buf), "%s:%d", ip, ntohs(addr.sin_port));
        return std::string(buf);
    }

}
