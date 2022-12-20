#pragma once

#include "win/sync.h"
#include "core/inc.h"
#include "core/event.h"
#include "win/tpp_thread_pool.h"

struct _OVERLAPPED;

namespace win32
{
    enum class pipe_mode
    {
        none = 0,
        read = 0x0001,
        write = 0x0002,
        async = 0x0004,
        readwrite = read | write,
    };
    typedef core::bitflag<pipe_mode> pipe_modes;

    class pipe
    {
    public:
        pipe();
        ~pipe();

        core::error_e state() const { return _state; }
        bool valid() const { return !!_pipe; }

        core::error_e create(std::string name, pipe_modes mode);
        core::error_e open(std::string name, pipe_modes modes);
        core::error_e close();

        core::error_e read(byte_t * buffer, size_t nbytes);
        core::error_e write(const byte_t * buffer, size_t nbytes);
        core::error_e recv(byte_t * buffer, size_t nbytes);
        core::error_e send(const byte_t * buffer, size_t nbytes);

    public:
        // 管道另一端连接了
        core::event<void(core::error_e err)> arrived;
        core::event<void(core::error_e err, size_t nbytes)> recved;
        core::event<void(core::error_e err, size_t nbytes)> sended;

    private:
        std::string _name;
        std::atomic<core::error_e> _state = core::error_broken;
        std::shared_ptr<void> _pipe;

        pipe_modes _modes = pipe_mode::none;
        std::shared_ptr<win32::tpp_iocp> _tpp_worker;

        std::shared_ptr<void> _ovlp_recv;
        std::shared_ptr<void> _ovlp_send;

        std::shared_ptr<win32::tpp_ovlp> _listen_worker;
    };
}
