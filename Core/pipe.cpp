#include "win/pipe.h"
#include "win/win32.h"
#include "core/strings.h"
#include "core/logger.h"

namespace win32
{
    struct ipie_iocp_context
    {
        HANDLE hEvent;
    };

    pipe::pipe()
    {
        
    }

    pipe::~pipe()
    {
		close();
    }

    core::error_e pipe::create(std::string name, pipe_modes modes)
    {
        if (_pipe)
            return core::error_exists;

        std::wstring namew = L"\\\\.\\pipe\\" + core::u8_ucs2(name);
        core::bitflag<uint32_t> openMode = FILE_FLAG_OVERLAPPED;
        openMode.set(PIPE_ACCESS_INBOUND, modes.any(pipe_mode::read));
        openMode.set(PIPE_ACCESS_OUTBOUND, modes.any(pipe_mode::write));
        openMode.set(FILE_FLAG_OVERLAPPED, modes.any(pipe_mode::async));
        HANDLE hPipe = CreateNamedPipeW(namew.c_str(), openMode, PIPE_TYPE_BYTE, PIPE_UNLIMITED_INSTANCES, 1024, 1024, 0, NULL);
        if (!hPipe)
            return win32::winerr();

        _pipe.reset(hPipe, CloseHandle);
        _modes = modes;
        _name = name;
        _state = core::error_broken;

        _listen_worker = win32::thread_pool::instance().create_ovlp();
        _listen_worker->done += [this](core::error_e state) { _state = core::error_ok;  arrived(state); };
        _listen_worker->wait();

        _tpp_worker = win32::thread_pool::instance().create_iocp();
        _tpp_worker->iocp_done += [this](core::error_e state, void * ovlp)
        {
            _state = state;
            OVERLAPPED * overlapped = (OVERLAPPED *)(ovlp);
            if(overlapped == _ovlp_recv.get())
            {
                DWORD dwRecv= 0;
                BOOL err_ovlp = GetOverlappedResult(_pipe.get(), overlapped, &dwRecv, false);
                recved(state, dwRecv);
            }
            else if (overlapped == _ovlp_send.get())
            {
                DWORD dwSend = 0;
                BOOL err_ovlp = GetOverlappedResult(_pipe.get(), overlapped, &dwSend, false);
                sended(state, dwSend);
            }
            else
            {
                
            }
        };

        OVERLAPPED * ovlp = (OVERLAPPED *)(_listen_worker->ovlp());
        bool failed = ConnectNamedPipe(hPipe, ovlp);
        if (failed)
            return win32::winerr();

        auto err = GetLastError();
        if (err == ERROR_PIPE_CONNECTED)
        {
            SetEvent(ovlp->hEvent);
        }
        else if (err == ERROR_IO_PENDING)
        {
        }
        else
            return win32::winerr_val(err);
        return core::error_ok;
    }

    core::error_e pipe::open(std::string name, pipe_modes modes)
    {
        std::wstring namew = L"\\\\.\\pipe\\" + core::u8_ucs2(name);
        core::bitflag<uint32_t> access = nullptr;
		access.set(GENERIC_READ, modes.any(pipe_mode::read));
		access.set(GENERIC_WRITE, modes.any(pipe_mode::write));

        HANDLE hPipe = CreateFileW(namew.c_str(), access, 0, NULL, OPEN_EXISTING, (modes & pipe_mode::async) ? FILE_FLAG_OVERLAPPED : 0, NULL);
        if (hPipe == INVALID_HANDLE_VALUE)
            return win32::winerr();

        _pipe = std::shared_ptr<byte_t>((byte_t *)hPipe, [](byte_t * ptr) { ::CloseHandle((HANDLE)ptr); });
        _modes = modes;
        _name = name;
        _state = core::error_ok;

        _tpp_worker = win32::thread_pool::instance().create_iocp();
        _tpp_worker->iocp_done += [this](core::error_e state, void * ovlp)
        {
            _state = state;
            OVERLAPPED * overlapped = (OVERLAPPED *)(ovlp);
            if (overlapped == _ovlp_recv.get())
            {
                DWORD dwRecv = 0;
                BOOL err_ovlp = GetOverlappedResult(_pipe.get(), overlapped, &dwRecv, false);
                recved(state, dwRecv);
            }
            else if (overlapped == _ovlp_send.get())
            {
                DWORD dwSend = 0;
                BOOL err_ovlp = GetOverlappedResult(_pipe.get(), overlapped, &dwSend, false);
                sended(state, dwSend);
            }
            else
            {

            }
        };

        return core::error_ok;
    }

    core::error_e pipe::close()
    {
        if (!_pipe)
            return core::error_ok;

        _listen_worker.reset();

		if (_tpp_worker)
		{
			_tpp_worker->stop_thread();
			_tpp_worker.reset();
		}

        _ovlp_recv.reset();
        _ovlp_send.reset();
        _pipe.reset();
        _modes = win32::pipe_mode::none;
        _name.clear();
        _state = core::error_broken;

        return core::error_ok;
    }

    core::error_e pipe::read(byte_t * buffer, size_t nbytes)
    {
        if (!_pipe)
            return core::error_state;

        if (!(_modes & pipe_mode::read))
            return core::error_access;

        uint32_t nbRead = 0;
        bool bSuccess = ReadFile(_pipe.get(), buffer, (DWORD)nbytes, (DWORD *)&nbRead, NULL);
        if (bSuccess)
            return core::error_ok;
        return win32::winerr();
    }

    core::error_e pipe::write(const byte_t * buffer, size_t nbytes)
    {
        if (!_pipe)
            return core::error_state;

        if (!(_modes & pipe_mode::write))
            return core::error_access;

        uint32_t nbWriten = 0;
        bool bSuccess = WriteFile(_pipe.get(), buffer, (DWORD)nbytes, (DWORD *)&nbWriten, NULL);
        if (bSuccess)
            return core::error_ok;
        return win32::winerr();
    }

    core::error_e pipe::recv(byte_t * buffer, size_t nbytes)
    {
        if (!_pipe)
            return core::error_state;

        if (!(_modes & pipe_mode::read) || !(_modes & pipe_mode::async))
            return core::error_access;

        if(!_ovlp_recv)
        {
            auto overlapped = std::make_shared<OVERLAPPED>();
            *overlapped = {};
            _ovlp_recv = overlapped;
        }

        _tpp_worker->wait(_pipe);
        OVERLAPPED * ovlp = (OVERLAPPED *)(_ovlp_recv.get());
        DWORD dwRead = 0;
        bool bSuccess = ReadFile(_pipe.get(), buffer, (DWORD)nbytes, &dwRead, ovlp);
        if (bSuccess)
        {
            //std::async([this]() { recved(core::error_ok); });
            return core::error_ok;
        }

        return core::error_ok;
    }

    core::error_e pipe::send(const byte_t * buffer, size_t nbytes)
    {
        if (!_pipe)
            return core::error_state;

        if (!(_modes & pipe_mode::write) || !(_modes & pipe_mode::async))
            return core::error_access;

        if (!_ovlp_send)
        {
            auto overlapped = std::make_shared<OVERLAPPED>();
            *overlapped = {};
            _ovlp_send = overlapped;
        }

        _tpp_worker->wait(_pipe);
        OVERLAPPED * ovlp = (OVERLAPPED *)(_ovlp_send.get());
        DWORD nbWriten = 0;
        bool bSuccess = WriteFile(_pipe.get(), buffer, (DWORD)nbytes, &nbWriten, ovlp);
        if (bSuccess)
        {
            //std::async([this]() { recved(core::error_ok); });
            return core::error_ok;
        }

        return core::error_ok;
    }
}
#ifdef WINDOWS_PLATFORM_TYPES_GUARD
#include "Windows/HideWindowsPlatformTypes.h"
#endif