#include "core/strings.h"
#include "core/error.h"

#define ERROR_TAG(tag) tag, #tag

namespace core
{
    struct error_entry
    {
        int num;
        const char * tag;
        const char *str;
    };

    static const struct error_entry error_entries[] =
    {
        { ERROR_TAG(error_generic), "generic error" },
        { ERROR_TAG(error_inner), "inner error" },
        { ERROR_TAG(error_eof), "end of stream" },
        { ERROR_TAG(error_nullptr), "null pointer" },
        { ERROR_TAG(error_unreachable), "unreachable" },
        { ERROR_TAG(error_outofmemory), "out of memory" },
        { ERROR_TAG(error_args), "invalid args" },
        { ERROR_TAG(error_exists), "not exists" },
        { ERROR_TAG(error_access), "no access" },
        { ERROR_TAG(error_state), "invalid state" },
        { ERROR_TAG(error_io), "io error" },
        { ERROR_TAG(error_broken), "broken" },
        { ERROR_TAG(error_timeout), "timeout" },
        { ERROR_TAG(error_cancel), "cancel" },
        { ERROR_TAG(error_bad_data), "bad data" },
        { ERROR_TAG(error_bad_format), "bad format" },
        { ERROR_TAG(error_not_now), "not now" },
        { ERROR_TAG(error_not_supported), "not supported" },
        { ERROR_TAG(error_not_found), "not found" },
        { ERROR_TAG(error_not_implemented), "not implemented" },
        { ERROR_TAG(error_ok), "ok" },
        { ERROR_TAG(error_true), "true" },
        { ERROR_TAG(error_false), "false" },
        { ERROR_TAG(error_pendding), "pendding" },
    };

    const char * error_str(error_e err)
    {
        for (const error_entry & entry : error_entries)
        {
            if (err == entry.num)
                return entry.str;
        }

        return "unknown error";
    }
}
