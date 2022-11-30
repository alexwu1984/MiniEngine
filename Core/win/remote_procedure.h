#pragma once

#include "core/inc.h"
#include "win/shared_memory.h"
#include "win/sync.h"

namespace win32
{
    struct remote_procedure_header
    {
        uint32_t cmd = 0;
        uint32_t ret = 0;
        size_t raw_size = 0;
        size_t raw_length = 0;
        uint32_t server_index = 0;
        uint32_t client_index = 0;

        uint32_t param0 = 0;
        uint32_t param1 = 0;
        uint32_t param2 = 0;
        uint32_t param3 = 0;
    };

    class remote_procedure
    {
    public:
        remote_procedure() {}
        remote_procedure(std::string name, size_t raw_size);
        remote_procedure(std::string name);
        ~remote_procedure() {}

        core::error_e create(std::string name, size_t raw_size);
        core::error_e open(std::string name, std::chrono::milliseconds timeout, size_t retry);

        core::error_e call(uint32_t cmd, const byte_t * data, size_t nbytes, std::chrono::milliseconds timeout);

        const byte_t * raw_data() const { return _header.raw_data(); }
        size_t raw_length() const { return _header->raw_length; }
        core::error_e wait();
        core::error_e wait_for(std::chrono::milliseconds timeout);
        const remote_procedure_header & header() const { return *_header; }
        remote_procedure_header & header() { return *_header; }
        core::error_e finish(core::error_e ret, const byte_t * data, size_t nbytes);

        operator bool() const { return !!_header; }

    private:
        win32::signal _signal_server;
        win32::signal _signal_client;

        shared_memory<remote_procedure_header> _header;
    };
}
