#pragma once

#include "win/shared_memory.h"
#include "core/event.h"
#include "win/tpp_thread_pool.h"

namespace win
{
    struct swapchain_header
    {
        int session = 0;
    };

    struct swapchain_buffers
    {
        int format = 0;
        size_t width = 0;
        size_t height = 0;
        size_t stride = 0;

        long write = 0;
        long temp = 0;
        long read = 0;
        long align__ = 0;
    };

    class frame_swapchain
    {
    public:
        frame_swapchain();
        ~frame_swapchain();

        bool connected() const { return _header.valid(); }
        core::error_e close();

        core::error_e create(std::string name);
        byte_t *  write(size_t width, size_t height);
        void present();

        core::error_e open(std::string name);
        std::tuple<const byte_t *, size_t, size_t> read();

        core::error_e recv();

    public:
        core::event<void(core::error_e err)> arrived;

    private:
        std::mutex _mtx;
        std::string _name;
        // 0 是无效的 ssession，从 1 开始有效
        std::atomic_uint32_t _session = 0;
        std::atomic<core::error_e> _state = core::error_broken;
        win32::shared_memory<swapchain_header> _header;
        win32::shared_memory<swapchain_buffers> _buffers;
        std::shared_ptr<void> _event;
        std::shared_ptr<win32::tpp_waiter> _waiter;
    };
}
