#include "pch.h"
#include "src/net/socks5.hpp"
#include "src/core/logger.hpp"
#include "src/core/utils.hpp"

namespace droute {

    bool Socks5ReadReply(SOCKET s, Socks5Reply& out) {
        uint8_t header[4];
        if (!RecvAll(s, header, 4)) {
            LOG_ERROR("socks5 handshake: failed to read reply header");
            return false;
        }
        if (header[0] != 0x05) {
            LOG_ERROR("socks5 handshake: invalid version 0x%02X", header[0]);
            return false;
        }

        out.rep = header[1];
        out.atyp = header[3];

        uint8_t addrLen = 0;
        switch (out.atyp) {
        case 0x01:
            addrLen = 4 + 2;
            break;
        case 0x03: {
            uint8_t domainLen;
            if (!RecvAll(s, &domainLen, 1)) {
                LOG_ERROR("socks5 handshake: failed to read domain length");
                return false;
            }
            addrLen = domainLen + 2;
            break;
        }
        case 0x04:
            addrLen = 16 + 2;
            break;
        default:
            LOG_ERROR("socks5 handshake: unsupported ATYP 0x%02X", out.atyp);
            return false;
        }

        std::vector<uint8_t> addrBuf(addrLen);
        if (!RecvAll(s, addrBuf.data(), addrLen)) {
            LOG_ERROR("socks5 handshake: failed to read reply address");
            return false;
        }

        out.boundAddr = {};
        out.boundAddr.sin_family = AF_INET;

        if (out.atyp == 0x01) {
            memcpy(&out.boundAddr.sin_addr.s_addr, addrBuf.data(), 4);
            memcpy(&out.boundAddr.sin_port, addrBuf.data() + 4, 2);
        } else if (out.atyp == 0x03) {
            uint8_t domainLen = addrLen - 2;
            memcpy(&out.boundAddr.sin_port, addrBuf.data() + domainLen, 2);
        } else {
            memcpy(&out.boundAddr.sin_port, addrBuf.data() + 16, 2);
        }

        return true;
    }

    bool Socks5Handshake(SOCKET s, const char* user, const char* pass) {
        bool hasAuth = user && user[0] && pass && pass[0];

        uint8_t req[4] = { 0x05, hasAuth ? uint8_t(0x02) : uint8_t(0x01), 0x00, uint8_t(0x02) };
        int reqLen = hasAuth ? 4 : 3;

        if (!SendAll(s, req, reqLen)) {
            LOG_ERROR("socks5 handshake: failed to send greeting");
            return false;
        }

        uint8_t resp[2];
        if (!RecvAll(s, resp, 2)) {
            LOG_ERROR("socks5 handshake: failed to recv greeting response");
            return false;
        }

        if (resp[0] != 0x05) {
            LOG_ERROR("socks5 handshake: invalid version 0x%02X", resp[0]);
            return false;
        }

        if (resp[1] == 0x02) {
            if (!hasAuth) {
                LOG_ERROR("socks5 handshake: server demands auth but none configured");
                return false;
            }
            size_t ulen = strlen(user);
            size_t plen = strlen(pass);
            if (ulen > 255 || plen > 255) {
                LOG_ERROR("socks5 handshake: credentials too long");
                return false;
            }

            uint8_t auth[1 + 1 + 255 + 1 + 255];
            size_t pos = 0;
            auth[pos++] = 0x01;
            auth[pos++] = static_cast<uint8_t>(ulen);
            memcpy(&auth[pos], user, ulen); pos += ulen;
            auth[pos++] = static_cast<uint8_t>(plen);
            memcpy(&auth[pos], pass, plen); pos += plen;

            if (!SendAll(s, auth, static_cast<int>(pos))) {
                LOG_ERROR("socks5 auth: failed to send credentials");
                return false;
            }
            uint8_t aresp[2];
            if (!RecvAll(s, aresp, 2)) {
                LOG_ERROR("socks5 auth: failed to recv auth response");
                return false;
            }
            if (aresp[0] != 0x01 || aresp[1] != 0x00) {
                LOG_ERROR("socks5 auth failed");
                return false;
            }
        } else if (resp[1] == 0x00) {
        } else {
            LOG_ERROR("socks5 handshake: unsupported method 0x%02X", resp[1]);
            return false;
        }

        return true;
    }

    bool Socks5RequestConnect(SOCKET s, const sockaddr_in& target) {
        uint8_t req[10] = { 0x05, 0x01, 0x00, 0x01 };
        memcpy(req + 4, &target.sin_addr.s_addr, 4);
        memcpy(req + 8, &target.sin_port, 2);

        if (!SendAll(s, req, 10)) {
            LOG_ERROR("socks5 connect: failed to send request");
            return false;
        }

        Socks5Reply reply;
        if (!Socks5ReadReply(s, reply)) {
            return false;
        }
        if (reply.rep != 0x00) {
            LOG_ERROR("socks5 connect rejected: rep=0x%02X", reply.rep);
            return false;
        }

        return true;
    }

    bool Socks5RequestUdpAssociate(SOCKET ctrl, sockaddr_in& outRelay) {
        uint8_t req[10] = { 0x05, 0x03, 0x00, 0x01, 0, 0, 0, 0, 0, 0 };
        if (!SendAll(ctrl, req, 10)) {
            LOG_ERROR("socks5 udp associate: failed to send request");
            return false;
        }

        Socks5Reply reply;
        if (!Socks5ReadReply(ctrl, reply)) {
            return false;
        }
        if (reply.rep != 0x00) {
            LOG_ERROR("socks5 udp associate rejected: rep=0x%02X", reply.rep);
            return false;
        }

        outRelay = reply.boundAddr;
        return true;
    }

    std::vector<uint8_t> Socks5WrapUdp(const sockaddr_in& dst, const void* payload, int len) {
        std::vector<uint8_t> out;
        out.reserve(SOCKS5_UDP_HEADER_SIZE + len);
        out.push_back(0x00);
        out.push_back(0x00);
        out.push_back(0x00);
        out.push_back(0x01);
        uint32_t ip = dst.sin_addr.s_addr;
        out.push_back(static_cast<uint8_t>(ip & 0xFF));
        out.push_back(static_cast<uint8_t>((ip >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((ip >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((ip >> 24) & 0xFF));
        uint16_t port = dst.sin_port;
        out.push_back(static_cast<uint8_t>(port & 0xFF));
        out.push_back(static_cast<uint8_t>((port >> 8) & 0xFF));
        const uint8_t* p = static_cast<const uint8_t*>(payload);
        out.insert(out.end(), p, p + len);
        return out;
    }

    bool Socks5UnwrapUdp(const void* data, int len, sockaddr_in& outSrc, const void*& outPayload, int& outPayloadLen) {
        if (len < SOCKS5_UDP_HEADER_SIZE) return false;
        const uint8_t* p = static_cast<const uint8_t*>(data);
        if (p[0] != 0x00 || p[1] != 0x00) return false;
        if (p[3] != 0x01) return false;

        memset(&outSrc, 0, sizeof(outSrc));
        outSrc.sin_family = AF_INET;
        outSrc.sin_addr.s_addr = (uint32_t(p[4])) | (uint32_t(p[5]) << 8) | (uint32_t(p[6]) << 16) | (uint32_t(p[7]) << 24);
        outSrc.sin_port = uint16_t(p[8]) | (uint16_t(p[9]) << 8);
        outPayload = p + SOCKS5_UDP_HEADER_SIZE;
        outPayloadLen = len - SOCKS5_UDP_HEADER_SIZE;
        return true;
    }

}
