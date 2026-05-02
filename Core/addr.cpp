#include "network/addr.h"

#include "win/win32.h"
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <IPHlpApi.h>
#pragma comment(lib, "Iphlpapi.lib")

namespace network
{
    std::string localhostname()
    {
        char name[256] = {};
        int nlen = gethostname(name, 255);
        return std::string(name, nlen);
    }

    std::vector<netaddr> localaddrs()
    {
        return host2addrs(localhostname());
    }

    std::vector<netaddr> host2addrs(const std::string & hostname)
    {
        struct addrinfo hints = {};
        hints.ai_flags = 0/*AI_NUMERICHOST*/;
        hints.ai_family = AF_INET;
        hints.ai_socktype = 0;
        hints.ai_protocol = 0;
        struct addrinfo * res = nullptr;
        int err = getaddrinfo(hostname.c_str(), NULL, &hints, &res);
        if (!err && res)
        {
            std::vector<netaddr> vec;
            struct addrinfo * ai = res;
            while (ai)
            {
                if (ai->ai_addrlen == sizeof(sockaddr_in))
                {
                    const sockaddr_in & si = *reinterpret_cast<const sockaddr_in *>(ai->ai_addr);
                    netaddr addr = { { si.sin_addr.S_un.S_un_b.s_b1, si.sin_addr.S_un.S_un_b.s_b2, si.sin_addr.S_un.S_un_b.s_b3, si.sin_addr.S_un.S_un_b.s_b4 } };
                    vec.emplace_back(addr);
                }
                ai = ai->ai_next;
            }
            freeaddrinfo(res);
            return vec;
        }
        else
        {
            return {};
        }
    }

    std::vector<adapter_desc> adapterdescs()
    {
        static const size_t NUM_MAX = 128;
        std::unique_ptr<IP_ADAPTER_INFO, std::default_delete<IP_ADAPTER_INFO[]>> iais(new IP_ADAPTER_INFO[NUM_MAX]);
        unsigned long nbytes = sizeof(IP_ADAPTER_INFO) * NUM_MAX;
        int err = GetAdaptersInfo(iais.get(), &nbytes);
        if (err)
            return {};

        IP_ADAPTER_INFO * piai = iais.get();
        std::vector<adapter_desc> descs;
        while (piai)
        {
            adapter_desc desc;
            desc.identification = std::string(piai->AdapterName);
            desc.description = std::string(piai->Description);
            std::memcpy(desc.mac.data(), piai->Address, 6);

            auto ip = &(piai->IpAddressList);
            while (ip)
            {
                sockaddr_in sa;
                inet_pton(AF_INET, static_cast<const char *>(ip->IpAddress.String), &(sa.sin_addr));
                adapteraddr addr;
                addr.ip = { { sa.sin_addr.S_un.S_un_b.s_b1, sa.sin_addr.S_un.S_un_b.s_b2, sa.sin_addr.S_un.S_un_b.s_b3, sa.sin_addr.S_un.S_un_b.s_b4 } };
                inet_pton(AF_INET, static_cast<const char *>(ip->IpMask.String), &(sa.sin_addr));
                addr.mask = { { sa.sin_addr.S_un.S_un_b.s_b1, sa.sin_addr.S_un.S_un_b.s_b2, sa.sin_addr.S_un.S_un_b.s_b3, sa.sin_addr.S_un.S_un_b.s_b4 } };
                desc.addrs.push_back(addr);
                ip = ip->Next;
            }

            auto gateway = &(piai->GatewayList);
            while (gateway)
            {
                sockaddr_in sa;
                inet_pton(AF_INET, static_cast<const char *>(gateway->IpAddress.String), &(sa.sin_addr));
                adapteraddr addr;
                addr.ip = { { sa.sin_addr.S_un.S_un_b.s_b1, sa.sin_addr.S_un.S_un_b.s_b2, sa.sin_addr.S_un.S_un_b.s_b3, sa.sin_addr.S_un.S_un_b.s_b4 } };
                inet_pton(AF_INET, static_cast<const char *>(gateway->IpMask.String), &(sa.sin_addr));
                addr.mask = { { sa.sin_addr.S_un.S_un_b.s_b1, sa.sin_addr.S_un.S_un_b.s_b2, sa.sin_addr.S_un.S_un_b.s_b3, sa.sin_addr.S_un.S_un_b.s_b4 } };
                desc.gateways.push_back(addr);
                gateway = gateway->Next;
            }
            descs.push_back(desc);
            piai = piai->Next;
        }

        return descs;
    }
}
