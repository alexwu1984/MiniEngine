#pragma once

#include "core/inc.h"

namespace network
{
    typedef int32_t port_t;
    struct netaddr
    {
        std::array<uint8_t, 4> v4;
    };

    inline std::ostream & operator << (std::ostream & ost, const netaddr & ver)
    {
        return ost << uint32_t(ver.v4[0]) << "." << static_cast<uint32_t>(ver.v4[1]) << "." << static_cast<uint32_t>(ver.v4[2]) << "." << static_cast<uint32_t>(ver.v4[3]);
    }
    inline std::wostream & operator << (std::wostream & ost, const netaddr & ver)
    {
        return ost << uint32_t(ver.v4[0]) << "." << static_cast<uint32_t>(ver.v4[1]) << "." << static_cast<uint32_t>(ver.v4[2]) << "." << static_cast<uint32_t>(ver.v4[3]);
    }

    struct adapteraddr
    {
        netaddr ip = {};
        netaddr mask = {};
    };

    struct adapter_desc
    {
        std::string identification;
        std::string description;
        std::array<uint8_t, 6> mac = {};
        std::vector<adapteraddr> addrs;
        std::vector<adapteraddr> gateways;
    };

    std::string localhostname();
    std::vector<netaddr> localaddrs();
    std::vector<netaddr> host2addrs(const std::string & hostname);
    std::vector<adapter_desc> adapterdescs();
}
