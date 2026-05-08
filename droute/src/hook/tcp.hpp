#pragma once

#include "src/hook/hooks.hpp"

namespace droute {

    int ConnectToProxy(SOCKET s);
    int Socks5ProxyConnect(SOCKET s, const sockaddr_in& target);
    int ConnectViaProxy(SOCKET s, const sockaddr_in* target);

}
