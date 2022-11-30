#include "network/socket.h"
#include "win/win32.h"

#include "core/logger.h"
#include <WinSock2.h>
#include <WS2tcpip.h>

namespace network
{
    int32_t _socket_type(socket_type type)
    {
        switch (type)
        {
        case socket_type::steam: return SOCK_STREAM;
        case socket_type::dgram: return SOCK_DGRAM;
        default: return SOCK_STREAM;
        }
    }

    socket::socket()
    {

    }

    socket::socket(socketid_t id) :_id(id)
    {
        if (id)
            _state = core::error_ok;
    }

    socket::socket(std::string host, port_t port, std::chrono::microseconds timeout)
    {
        connect(host, port, timeout);
    }

    socket::socket(netaddr addr, port_t port, std::chrono::microseconds timeout)
    {
        connect(addr, port, timeout);
    }

    socket::~socket()
    {
        close();
    }

    core::error_e socket::state() const
    {
        return _state;
    }

    core::error_e socket::set_timeout(std::chrono::microseconds timeout_read, std::chrono::microseconds timeout_write)
    {
        _timeout_read = timeout_read;
        _timeout_send = timeout_write;
        if (_id != invalid_socketid)
        {
            timeval tv_read = { static_cast<long>(_timeout_read.count() / 1000000), static_cast<long>(_timeout_read.count() % 1000000) };
            setsockopt(_id, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&tv_read), sizeof(timeval));
            timeval tv_send = { static_cast<long>(_timeout_send.count() / 1000000), static_cast<long>(_timeout_send.count() % 1000000) };
            setsockopt(_id, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&tv_send), sizeof(timeval));
        }
        return core::error_ok;
    }

    core::error_e socket::connect(std::string host, port_t port, std::chrono::microseconds timeout)
    {
        std::vector<netaddr> addrs = host2addrs(host);
        if (addrs.empty())
            return core::error_unreachable;
        return connect(addrs[std::random_device()() % addrs.size()], port, timeout);
    }

    core::error_e socket::connect(netaddr addr, port_t port, std::chrono::microseconds timeout)
    {
        if (_state != core::error_broken)
            return core::error_state;

        if (_id == invalid_socketid)
        {
            _id = ::socket(AF_INET, _socket_type(_type), 0);
            if (_id == invalid_socketid)
                return core::error_generic;

            uint32_t to_read = _timeout_read.count() / 1000;
            uint32_t to_send = _timeout_send.count() / 1000;
            setsockopt(_id, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&to_read), sizeof(timeval));
            setsockopt(_id, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&to_send), sizeof(timeval));
        }

        sockaddr_in saddr = {};
        saddr.sin_family = 2;
        saddr.sin_port = ntohs((u_short)port);
        saddr.sin_addr.S_un.S_un_b.s_b1 = addr.v4[0];
        saddr.sin_addr.S_un.S_un_b.s_b2 = addr.v4[1];
        saddr.sin_addr.S_un.S_un_b.s_b3 = addr.v4[2];
        saddr.sin_addr.S_un.S_un_b.s_b4 = addr.v4[3];
        if (::connect(_id, (sockaddr *)&saddr, sizeof(sockaddr_in)) == -1)
        {
            uint32_t err = WSAGetLastError();
            return core::error_generic;
        }
        _state = core::error_ok;
        return core::error_ok;
    }

    void socket::close()
    {
        if (_id != invalid_socketid)
        {
            closesocket(_id);
            _id = invalid_socketid;
            _state = core::error_broken;
        }
    }

    core::error_e socket::select(std::chrono::microseconds timeout)
    {
        if (_state != core::error_ok)
            return core::error_state;

        fd_set readSets = {};
        fd_set writeSets = {};
        fd_set errorSets = {};
        FD_SET(_id, &readSets);
        FD_SET(_id, &errorSets);

        timeval tv = { static_cast<long>(timeout.count() / 1000000), static_cast<long>(timeout.count() % 1000000) };
        int ret = ::select(0, &readSets, &writeSets, &errorSets, &tv);
        if (ret == 0)
            return core::error_timeout;

        if (ret < 0)
        {
            int wserr = WSAGetLastError();
            return core::error_generic;
        }

        if (FD_ISSET(_id, &errorSets))
        {
            assert(false);
            return core::error_generic;
        }
        return core::error_ok;
    }

    std::tuple<core::error_e, int64_t> socket::write(const byte_t * buffer, int64_t nbytes)
    {
        if (_id == invalid_socketid)
            return { core::error_state, 0 };

        if (!buffer || !nbytes)
            return { core::error_ok, 0 };

        int ret = ::send(_id, reinterpret_cast<const char *>(buffer), static_cast<int32_t>(nbytes), 0);
        if (ret < 0)
        {
            return { core::error_io, 0 };
        }
        else
            return { core::error_ok, ret };
    }

    std::tuple<core::error_e, int64_t> socket::read(byte_t * buffer, int64_t nbytes)
    {
        if (_id == invalid_socketid || _state != core::error_ok)
            return { core::error_state, 0 };

        int32_t ret = ::recv(_id, reinterpret_cast<char *>(buffer), static_cast<int32_t>(nbytes), 0);
        if (ret < 0)
        {
            auto ws_error = WSAGetLastError();
            if (ws_error == WSAETIMEDOUT)
                return { core::error_timeout, 0 };

            return { core::error_io, 0 };
        }
        else if (ret == 0)
            return { core::error_broken, 0 };
        else
            return { core::error_ok, ret };
    }

    core::error_e socket::recv(byte_t * buffer, size_t nbytes)
    {
        if (!_recv_worker)
        {
            _recv_worker = win32::thread_pool::instance().create_ovlp();
            _recv_worker->done += [this](core::error_e state)
            {
                _state = state;
                OVERLAPPED * ovlp = (OVERLAPPED *)(_recv_worker->ovlp());
                DWORD dwRead = 0;
                auto err_ovlp = WSAGetOverlappedResult(_id, ovlp, &dwRead, false, reinterpret_cast<DWORD *>(&_recv_flags));
                if (!err_ovlp)
                    recved(win32::winerr_val(WSAGetLastError()), 0);
                else
                    recved(state, dwRead);
            };
        }

        if (!_recv_buffer)
            _recv_buffer = std::make_shared<WSABUF>();

        WSABUF * wsabuf = static_cast<WSABUF *>(_recv_buffer.get());
        wsabuf->buf = reinterpret_cast<char *>(buffer);
        wsabuf->len = nbytes;

        OVERLAPPED * ovlp = (OVERLAPPED *)(_recv_worker->ovlp());
        _recv_worker->wait();

        DWORD nbRead = 0;
        auto err_recv = WSARecv(_id, wsabuf, 1, &nbRead, reinterpret_cast<DWORD *>(&_recv_flags), ovlp, NULL);
        if (err_recv == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
            return core::error_generic;
        return core::error_ok;
    }
}
#ifdef WINDOWS_PLATFORM_TYPES_GUARD
#include "Windows/HideWindowsPlatformTypes.h"
#endif