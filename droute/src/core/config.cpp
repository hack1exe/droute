#include "pch.h"
#include "src/core/config.hpp"
#include "src/core/utils.hpp"

namespace droute {

    Config g_cfg;
    sockaddr_in g_proxyAddr;

    bool Config::Load() {
        HKEY hKey = nullptr;
        LONG res = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\droute", 0, KEY_READ, &hKey);
        if (res != ERROR_SUCCESS) {
            LOG_WARN("registry key not found, using defaults");
            return false;
        }

        auto ReadString = [&](const char* name, std::string& out) -> bool {
            char buf[512] = {};
            DWORD type = 0, size = sizeof(buf);
            if (RegQueryValueExA(hKey, name, nullptr, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS && type == REG_SZ) {
                out = buf;
                return true;
            }
            return false;
        };

        auto ReadDword = [&](const char* name, uint32_t& out) -> bool {
            DWORD type = 0, val = 0, size = sizeof(val);
            if (RegQueryValueExA(hKey, name, nullptr, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS && type == REG_DWORD) {
                out = val;
                return true;
            }
            return false;
        };

        ReadString("Host", host);
        uint32_t portTmp = port;
        if (ReadDword("Port", portTmp)) port = static_cast<uint16_t>(portTmp);
        ReadString("User", user);
        ReadString("Password", password);

        uint32_t tmp;
        if (ReadDword("ConnectTimeout", tmp))       connectTimeout = tmp;
        if (ReadDword("ReconnectInterval", tmp))    reconnectInterval = tmp;
        if (ReadDword("RetryTimeout", tmp))    retryTimeout = tmp;
        if (ReadDword("LogLevel", tmp)) {
            if (tmp <= 4) logLevel = static_cast<LogLevel>(tmp);
        }

        RegCloseKey(hKey);

        memset(&g_proxyAddr, 0, sizeof(g_proxyAddr));
        g_proxyAddr.sin_family = AF_INET;
        g_proxyAddr.sin_port = htons(static_cast<uint16_t>(port));

        if (inet_pton(AF_INET, host.c_str(), &g_proxyAddr.sin_addr) == 1) {
        } else {
            addrinfo hints = {};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            addrinfo* result = nullptr;
            if (getaddrinfo(host.c_str(), nullptr, &hints, &result) == 0 && result) {
                g_proxyAddr.sin_addr = reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr;
                freeaddrinfo(result);
            } else {
                LOG_ERROR("failed to resolve '%s'", host.c_str());
                inet_pton(AF_INET, "127.0.0.1", &g_proxyAddr.sin_addr);
            }
        }

        LOG_INFO("proxy=%s auth=%s timeout=%d retry=%d",
                 AddrToString(g_proxyAddr).c_str(),
                 user.empty() ? "none" : "set",
                 connectTimeout, retryTimeout);
        return true;
    }

}
