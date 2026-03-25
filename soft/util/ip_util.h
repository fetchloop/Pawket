#ifndef _IP_H_
#define _IP_H_

#include <WinSock2.h>
#include <WS2tcpip.h>

namespace Pawket
{
    namespace IP_UTIL
    {
        in_addr get_local_ip()
        {
            char hostname[255];
            gethostname(hostname, 255);

            addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;

            addrinfo* results = nullptr;
            int addr_status = getaddrinfo(hostname, nullptr, &hints, &results);

            if (addr_status != 0 || !results)
                return in_addr{};

            in_addr addr = reinterpret_cast<sockaddr_in*>(results->ai_addr)->sin_addr;
            freeaddrinfo(results);

            return addr;
        }
    }
}

#endif