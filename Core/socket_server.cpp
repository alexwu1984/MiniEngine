#include "network/socket_server.h"

#include "core/logger.h"
#include "win/win32.h"
#include <WinSock2.h>
#include <WS2tcpip.h>

namespace network
{
    socket_server::socket_server()
    {
    }

    socket_server::~socket_server()
    {
        close();
    }

    core::error_e socket_server::state() const
    {
        return _state;
    }

    core::error_e socket_server::set_timeout(std::chrono::microseconds timeout_read, std::chrono::microseconds timeout_write)
    {
        _timeout_read = timeout_read;
        _timeout_send = timeout_write;
        if (_fd != invalid_socketid)
        {
            timeval tv_read = { static_cast<long>(_timeout_read.count() / 1000000), static_cast<long>(_timeout_read.count() % 1000000) };
            setsockopt(_fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&tv_read), sizeof(timeval));
            timeval tv_send = { static_cast<long>(_timeout_send.count() / 1000000), static_cast<long>(_timeout_send.count() % 1000000) };
            setsockopt(_fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&tv_send), sizeof(timeval));
        }
        return core::error_ok;
    }

    core::error_e socket_server::listen(std::string host, port_t port, std::chrono::microseconds timeout)
    {
        std::vector<netaddr> addrs = host2addrs(host);
        if (addrs.empty())
            return core::error_unreachable;
        return listen(addrs[std::random_device()() % addrs.size()], port, timeout);
    }

    core::error_e socket_server::listen(netaddr addr, port_t port, std::chrono::microseconds timeout)
    {
        if (_state != core::error_broken)
            return core::error_state;

        if (_fd == invalid_socketid)
        {
            _fd = ::socket(AF_INET, _socket_type(_type), 0);
            if (_fd == invalid_socketid)
                return core::error_generic;

            struct timeval to_read { long(_timeout_read.count() / 1000),0};
			struct timeval to_send{ long(_timeout_send.count() / 1000),0};
            //uint32_t to_read = _timeout_read.count() / 1000;
            //uint32_t to_send = _timeout_send.count() / 1000;
            setsockopt(_fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&to_read), sizeof(timeval));
            setsockopt(_fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&to_send), sizeof(timeval));
        }

        sockaddr_in saddr = {};
        saddr.sin_family = 2;
        saddr.sin_port = ntohs((u_short)port);
        saddr.sin_addr.S_un.S_un_b.s_b1 = addr.v4[0];
        saddr.sin_addr.S_un.S_un_b.s_b2 = addr.v4[1];
        saddr.sin_addr.S_un.S_un_b.s_b3 = addr.v4[2];
        saddr.sin_addr.S_un.S_un_b.s_b4 = addr.v4[3];

        auto err_bind = ::bind(_fd, (SOCKADDR *)&saddr, sizeof(sockaddr_in));
        if (err_bind)
        {
            closesocket(_fd);
            return core::error_generic;
        }

        auto err_listen = ::listen(_fd, SOMAXCONN);
        if (err_listen)
        {
            closesocket(_fd);
            return core::error_generic;
        }

        HANDLE hevent = CreateEventW(NULL, false, false, NULL);
        if (!hevent)
            return win32::winerr();
        _event.reset(hevent, [](void * ptr) { CloseHandle(ptr); });

        if (!_waiter)
        {
            _waiter = win32::tpp_thread_pool::instance().create_waiter();
            _waiter->done += [this](core::error_e err)
            {
                auto fd = ::accept(_fd, nullptr, nullptr);
                auto client = std::make_shared<socket>(fd);
                connected(client);
                _waiter->wait(_event);
            };
        }
        _waiter->wait(_event);

        auto err_event = ::WSAEventSelect(_fd, _event.get(), FD_ACCEPT);
        if (err_event)
        {
            closesocket(_fd);
            return core::error_generic;
        }

        _state = core::error_ok;
        _port = port;
        _addr = addr;
        return core::error_ok;
    }

    std::tuple<core::error_e, port_t> socket_server::listen(std::string host, std::chrono::microseconds timeout)
    {
        std::vector<netaddr> addrs = host2addrs(host);
        if (addrs.empty())
            return { core::error_generic, 0 };
        return listen(addrs[std::random_device()() % addrs.size()], timeout);
    }

    std::tuple<core::error_e, port_t> socket_server::listen(netaddr addr, std::chrono::microseconds timeout)
    {
        if (_state != core::error_broken)
            return { core::error_generic, 0 };

        if (_fd == invalid_socketid)
        {
            _fd = ::socket(AF_INET, _socket_type(_type), 0);
            if (_fd == invalid_socketid)
                return { core::error_generic, 0 };

            //uint32_t to_read = _timeout_read.count() / 1000;
            //uint32_t to_send = _timeout_send.count() / 1000;
			struct timeval to_read { long(_timeout_read.count() / 1000), 0 };
			struct timeval to_send { long(_timeout_send.count() / 1000), 0 };
            setsockopt(_fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&to_read), sizeof(timeval));
            setsockopt(_fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&to_send), sizeof(timeval));
        }

        sockaddr_in saddr = {};
        saddr.sin_family = 2;
        saddr.sin_port = 0;
        saddr.sin_addr.S_un.S_un_b.s_b1 = addr.v4[0];
        saddr.sin_addr.S_un.S_un_b.s_b2 = addr.v4[1];
        saddr.sin_addr.S_un.S_un_b.s_b3 = addr.v4[2];
        saddr.sin_addr.S_un.S_un_b.s_b4 = addr.v4[3];

        auto err_bind = ::bind(_fd, (SOCKADDR *)&saddr, sizeof(sockaddr_in));
        if (err_bind)
        {
            closesocket(_fd);
            return { core::error_generic, 0 };
        }

        auto err_listen = ::listen(_fd, SOMAXCONN);
        if (err_listen)
        {
            closesocket(_fd);
            return { core::error_generic, 0 };
        }

        HANDLE hevent = CreateEventW(NULL, false, false, NULL);
        if (!hevent)
            return { win32::winerr(), 0 };
        _event.reset(hevent, [](void * ptr) { CloseHandle(ptr); });

        if (!_waiter)
        {
            _waiter = win32::tpp_thread_pool::instance().create_waiter();
            _waiter->done += [this](core::error_e err)
            {
                auto fd = ::accept(_fd, nullptr, nullptr);
                auto client = std::make_shared<socket>(fd);
                connected(client);
                _waiter->wait(_event);
            };
        }
        _waiter->wait(_event);

        auto err_event = ::WSAEventSelect(_fd, _event.get(), FD_ACCEPT);
        if (err_event)
        {
            closesocket(_fd);
            return { core::error_generic, 0 };
        }

        struct sockaddr_in sin;
        socklen_t len = sizeof(sin);
        if (getsockname(_fd, (struct sockaddr *)&sin, &len) == -1)
            return { core::error_generic, 0 };

        _state = core::error_ok;
        _port = ntohs(sin.sin_port);
        _addr = addr;
        return { core::error_ok, _port };
    }

    void socket_server::close()
    {
        if (_fd != invalid_socketid)
        {
            closesocket(_fd);
            _fd = invalid_socketid;
            _state = core::error_broken;
        }
    }
}
