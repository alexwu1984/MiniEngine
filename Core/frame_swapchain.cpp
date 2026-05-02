#include "win/frame_swapchain.h"
#include "win/win32.h"
#include "core/strings.h"
#include "core/system.h"

namespace win
{
    frame_swapchain::frame_swapchain()
    {
        
    }

    frame_swapchain::~frame_swapchain()
    {

    }

    core::error_e frame_swapchain::close()
    {
        std::lock_guard<std::mutex> l(_mtx);
        _session = 0;
        _state = core::error_broken;
        _waiter.reset();
        _buffers.close();
        _header.close();
        _event.reset();
        return core::error_ok;
    }

    core::error_e frame_swapchain::create(std::string name)
    {
        std::lock_guard<std::mutex> l(_mtx);
        std::string event_name = core::format(name, ".event");
        HANDLE hevent = CreateEventW(NULL, false, false, core::u8_ucs2(event_name).c_str());
        if (!hevent)
            return win32::winerr();
        _event.reset(hevent, [](void * ptr) { CloseHandle(ptr); });

        std::string header_name = core::format(name, ".header");
        auto err_header = _header.create(header_name, 0, win32::shared_mode::readwrite);
        if (err_header)
            return win32::winerr();

        _name = name;
        return core::error_ok;
    }

    byte_t * frame_swapchain::write(size_t width, size_t height)
    {
        std::lock_guard<std::mutex> l(_mtx);
        if (!_header)
            return nullptr;

        if (!_buffers || _buffers->width != width || _buffers->height != height)
        {
            _buffers.reset();

            ++_session;
            std::string buffer_name = core::format(_name, ".buffer.", _session);
            auto err_buffer = _buffers.create(buffer_name, width * height * 4 * 3, win32::shared_mode::readwrite);
            if (err_buffer)
                return nullptr;

            _buffers->width = width;
            _buffers->height = height;
            _buffers->write = 0;
            _buffers->temp = 1;
            _buffers->read = 2;

            _header->session = _session;
            _state = core::error_ok;
        }

        size_t bytes = _buffers->width * _buffers->height * 4;
        return _buffers.raw_data() + bytes * _buffers->write;
    }

    void frame_swapchain::present()
    {
        std::lock_guard<std::mutex> l(_mtx);
        if (!_event || !_buffers)
            return;

        _buffers->write = _InterlockedExchange(&_buffers->temp, _buffers->write);
        SetEvent(_event.get());
    }

    core::error_e frame_swapchain::open(std::string name)
    {
        std::lock_guard<std::mutex> l(_mtx);
        std::string header_name = core::format(name, ".header");
        std::string event_name = core::format(name, ".event");
        auto err_header = _header.open(header_name, win32::shared_mode::readwrite);
        if (err_header)
            return err_header;

        HANDLE hevent = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, false, core::u8_ucs2(event_name).c_str());
        if (!hevent)
            return win32::winerr();

        _event.reset(hevent, [](void * ptr) {  CloseHandle(ptr); });
        _state = core::error_ok;
        _name = name;
        return core::error_ok;
    }

    std::tuple<const byte_t *, size_t, size_t> frame_swapchain::read()
    {
        std::lock_guard<std::mutex> l(_mtx);
        if (!_header)
            return {};

        if (!_header->session)
            return {};

        if(_header->session != _session)
        {
            if (_name.empty())
                return {};

            std::string buffer_name = core::format(_name, ".buffer.", _header->session);
            auto err_buffer = _buffers.open(buffer_name, win32::shared_mode::readwrite);
            if (err_buffer)
                return {};
            _session = _header->session;
        }

        _buffers->read = _InterlockedExchange(&_buffers->temp, _buffers->read);
        size_t bytes = _buffers->width * _buffers->height * 4;
        return { _buffers.raw_data() + bytes * _buffers->read, _buffers->width, _buffers->height };
    }

    core::error_e frame_swapchain::recv()
    {
        std::lock_guard<std::mutex> l(_mtx);
        if (!_header)
            return core::error_state;

        if (!_waiter)
        {
            _waiter = win32::tpp_thread_pool::instance().create_waiter();
            _waiter->done += [this](core::error_e err) { arrived(err); };
        }
        _waiter->wait(_event);
        return core::error_ok;
    }
}

#ifdef WINDOWS_PLATFORM_TYPES_GUARD
#include "Windows/HideWindowsPlatformTypes.h"
#endif