#pragma once

#include "pch.h"

namespace droute {

    constexpr int SOCKS5_UDP_HEADER_SIZE = 10;

    struct Socks5Reply {
        uint8_t rep;
        uint8_t atyp;
        sockaddr_in boundAddr;
    };

    bool Socks5Handshake(SOCKET s, const char* user, const char* pass);
    bool Socks5RequestConnect(SOCKET s, const sockaddr_in& target);
    bool Socks5RequestUdpAssociate(SOCKET ctrl, sockaddr_in& outRelay);

    std::vector<uint8_t> Socks5WrapUdp(const sockaddr_in& dst, const void* payload, int len);
    bool Socks5UnwrapUdp(const void* data, int len, sockaddr_in& outSrc, const void*& outPayload, int& outPayloadLen);

}
