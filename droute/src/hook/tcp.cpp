#include "pch.h"
#include "src/hook/tcp.hpp"
#include "src/hook/hooks.hpp"
#include "src/core/config.hpp"
#include "src/core/logger.hpp"
#include "src/core/utils.hpp"
#include "src/net/socks5.hpp"

namespace droute {

    int ConnectToProxy(SOCKET s) {
        auto start = GetTickCount64();

        while (true) {
            int rc = Hooks::Real_connect(s, reinterpret_cast<const sockaddr*>(&g_proxyAddr), sizeof(g_proxyAddr));
            if (rc == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err == WSAEWOULDBLOCK) {
                    if (!WaitForWrite(s, static_cast<int>(g_cfg.connectTimeout))) {
                        if (GetTickCount64() - start >= g_cfg.retryTimeout) {
                            WSASetLastError(WSAETIMEDOUT);
                            return SOCKET_ERROR;
                        }
                        Sleep(g_cfg.reconnectInterval);
                        continue;
                    }
                    int soErr = 0; int soLen = sizeof(soErr);
                    getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soErr), &soLen);
                    if (soErr != 0) {
                        if (GetTickCount64() - start >= g_cfg.retryTimeout) {
                            WSASetLastError(soErr);
                            return SOCKET_ERROR;
                        }
                        Sleep(g_cfg.reconnectInterval);
                        continue;
                    }
                } else if (err == WSAECONNREFUSED || err == WSAETIMEDOUT || err == WSAENETUNREACH) {
                    if (GetTickCount64() - start >= g_cfg.retryTimeout) {
                        WSASetLastError(err);
                        return SOCKET_ERROR;
                    }
                    Sleep(g_cfg.reconnectInterval);
                    continue;
                } else {
                    return SOCKET_ERROR;
                }
            }

            return 0;
        }
    }

    int Socks5ProxyConnect(SOCKET s, const sockaddr_in& target) {
        if (!Socks5Handshake(s, g_cfg.user.c_str(), g_cfg.password.c_str())) {
            WSASetLastError(WSAECONNRESET);
            return SOCKET_ERROR;
        }
        if (!Socks5RequestConnect(s, target)) {
            WSASetLastError(WSAECONNRESET);
            return SOCKET_ERROR;
        }
        return 0;
    }

    int ConnectViaProxy(SOCKET s, const sockaddr_in* target) {
        if (ConnectToProxy(s) != 0) {
            return SOCKET_ERROR;
        }
        if (Socks5ProxyConnect(s, *target) != 0) {
            return SOCKET_ERROR;
        }
        return 0;
    }

    int WSAAPI Mine_connect(SOCKET s, const sockaddr* name, int namelen) {
        if (namelen < static_cast<int>(sizeof(sockaddr_in))) {
            return Hooks::Real_connect(s, name, namelen);
        }

        const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(name);
        if (addr->sin_family != AF_INET) {
            return Hooks::Real_connect(s, name, namelen);
        }

        if (addr->sin_addr.s_addr == 0) {
            return Hooks::Real_connect(s, name, namelen);
        }

        if (IsLocalAddr(*addr) || IsSameAddr(*addr, g_proxyAddr)) {
            return Hooks::Real_connect(s, name, namelen);
        }
        if (IsUdpSocket(s)) {
            return Hooks::Real_connect(s, name, namelen);
        }

        bool wasNonBlocking = false;
        {
            std::shared_lock<std::shared_mutex> lock(g_stateMutex);
            wasNonBlocking = g_nonBlockingSockets.count(s) != 0;
        }

        LOG_DEBUG("connect -> %s via proxy", AddrToString(*addr).c_str());

        int result = ConnectViaProxy(s, addr);
        if (result != 0) {
            int err = WSAGetLastError();
            LOG_WARN("connect -> %s failed: %d", AddrToString(*addr).c_str(), err);
            return SOCKET_ERROR;
        }

        LOG_INFO("connect -> %s via proxy", AddrToString(*addr).c_str());

        if (wasNonBlocking) {
            WSASetLastError(WSAEWOULDBLOCK);
            return SOCKET_ERROR;
        }
        return 0;
    }

}
