#pragma once

#include "core/inc.h"
#include "core/event.h"
#include "addr.h"
#include "socket.h"
#include "win/thread_pool.h"

namespace network
{
    class socket_server
    {
    public:
        socket_server();
        ~socket_server();

        core::error_e state() const;
        port_t port() const { return _port; }
        core::error_e set_timeout(std::chrono::microseconds timeout_read, std::chrono::microseconds timeout_write);
        core::error_e listen(std::string host, port_t port, std::chrono::microseconds timeout);
        core::error_e listen(netaddr addr, port_t port, std::chrono::microseconds timeout);
        std::tuple<core::error_e, port_t> listen(std::string host, std::chrono::microseconds timeout);
        std::tuple<core::error_e, port_t> listen(netaddr addr, std::chrono::microseconds timeout);
        std::tuple<core::error_e, netaddr, port_t> listen(port_t port, std::chrono::microseconds timeout);
        std::tuple<core::error_e, netaddr, port_t> listen(std::chrono::microseconds timeout);
        void close();

    protected:
        uintx_t _fd = invalid_socketid;
        socket_type _type = socket_type::steam;
        netaddr _addr = {};
        port_t _port = 0;
        core::error_e _state = core::error_broken;
        std::chrono::microseconds _timeout_read = 6s;
        std::chrono::microseconds _timeout_send = 6s;
        int32_t _ws_error = 0;

        std::shared_ptr<win32::tpp_ovlp> _listen_worker;

        std::shared_ptr<void> _event;
        std::shared_ptr<win32::tpp_waiter> _waiter;


    public:
        core::event<void(std::shared_ptr<socket> client)> connected;
    };
}
