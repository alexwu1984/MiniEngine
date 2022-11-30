#include "win/remote_procedure.h"
#include "win/win32.h"
#include "core/logger.h"

namespace win32
{
    remote_procedure::remote_procedure(std::string name, size_t raw_size)
    {
        create(name, raw_size);
    }

    remote_procedure::remote_procedure(std::string name)
    {
        open(name, 1000ms, 3600);
    }

    core::error_e remote_procedure::create(std::string name, size_t raw_size)
    {
        _header.create(name, raw_size, shared_mode::readwrite);
        _header->raw_size = raw_size;
        _signal_server.create(name + ".signal_server", false, false);
        while(auto err = _signal_client.open(name + ".signal_client"))
            std::this_thread::sleep_for(100ms);
        return core::error_ok;
    }

    core::error_e remote_procedure::open(std::string name, std::chrono::milliseconds timeout, size_t retry)
    {
        while (auto err = _signal_server.open(name + ".signal_server"))
        {
            if (!retry)
                return core::error_timeout;
            std::this_thread::sleep_for(timeout);
            --retry;
        }
        _header.open(name, shared_mode::readwrite);
        _signal_client.create(name + ".signal_client", false, false);
        return core::error_ok;
    }

    core::error_e remote_procedure::call(uint32_t cmd, const byte_t * data, size_t nbytes, std::chrono::milliseconds timeout)
    {
        if (nbytes > _header->raw_size)
            return core::error_outofmemory;
        std::memcpy(_header.raw_data(), data, nbytes);
        ++_header->server_index;
        _header->raw_length = nbytes;
        _header->cmd = cmd;
        _header->ret = core::error_not_implemented;
        _signal_server.set();
        if(auto err = _signal_client.wait_for(30s))
            return err;

        return (core::error_e)_header->ret;
    }

    core::error_e remote_procedure::wait()
    {
        auto err = _signal_server.wait();
        if (err)
            return err;

        _header->client_index = _header->server_index;
        return core::error_ok;
    }

    core::error_e remote_procedure::wait_for(std::chrono::milliseconds timeout)
    {
        return _signal_server.wait_for(timeout);
    }

    core::error_e remote_procedure::finish(core::error_e ret, const byte_t * data, size_t nbytes)
    {
        if (nbytes > _header->raw_size)
        {
            _header->raw_length = 0;
            _header->ret = core::error_outofmemory;
            _signal_client.set();
            return core::error_outofmemory;
        }
        std::memcpy(_header.raw_data(), data, nbytes);
        _header->raw_length = nbytes;
        _header->ret = ret;
        _signal_client.set();
        return core::error_ok;
    }
}
