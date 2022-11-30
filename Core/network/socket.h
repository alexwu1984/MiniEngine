#pragma once

#include "core/inc.h"
#include "core/event.h"
#include "addr.h"
#include "win/thread_pool.h"

namespace network
{
    typedef uintx_t socketid_t;
    const socketid_t invalid_socketid = static_cast<socketid_t>(~0);

    enum class socket_type
    {
        unknown = -1,
        steam = 0,
        dgram,
    };

    int32_t _socket_type(socket_type type);

    class socket
    {
    public:
        socket();
        explicit socket(socketid_t id);
        socket(std::string host, network::port_t port, std::chrono::microseconds timeout);
        socket(netaddr addr, port_t port, std::chrono::microseconds timeout);
        ~socket();

        socketid_t id() const { return _id; }
        core::error_e state() const;
        core::error_e set_timeout(std::chrono::microseconds timeout_read, std::chrono::microseconds timeout_write);
        core::error_e connect(std::string host, port_t port, std::chrono::microseconds timeout);
        core::error_e connect(netaddr addr, port_t port, std::chrono::microseconds timeout);
        void close();

        core::error_e select(std::chrono::microseconds timeout);

        std::tuple<core::error_e, int64_t> write(const byte_t * buffer, int64_t nbytes);
        std::tuple<core::error_e, int64_t> read(byte_t * buffer, int64_t nbytes);

        core::error_e recv(byte_t * buffer, size_t nbytes);

    public:
        core::event<void(core::error_e err, size_t nbytes)> recved;

    protected:
        socketid_t _id = invalid_socketid;
        socket_type _type = socket_type::steam;
        core::error_e _state = core::error_broken;
        std::chrono::microseconds _timeout_read = 6s;
        std::chrono::microseconds _timeout_send = 6s;
        int32_t _ws_error = 0;

        uint32_t _recv_flags = 0;
        std::shared_ptr<void> _recv_buffer;
        std::shared_ptr<win32::tpp_ovlp> _recv_worker;
    };
}
