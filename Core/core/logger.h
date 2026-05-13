#pragma once

#include "core/inc.h"
#include "core/strings.h"

namespace core
{
    // core::logger_fps(__FILE__, __LINE__, 1000)
    struct logger_period
    {
        logger_period(const char * file, int line, std::chrono::milliseconds period = 1s, size_t count = 1) : _t(file, line), _period(period), _count(count)
        {

        }

        bool ok() const;

        std::tuple<const char *, int> _t;
        std::chrono::milliseconds _period;
        size_t _count = 1000;
    };

    struct logger_fps : public logger_period
    {
        logger_fps(const char * file, int line, int fps) : logger_period(file, line, 1s, fps) {}
    };
}

namespace std
{
    inline std::basic_ostream<char, std::char_traits<char>> & operator<<(std::basic_ostream<char, std::char_traits<char>>&& _Ostr, const core::logger_period & fps)
    {
        if(_Ostr.good())
            fps.ok() ? _Ostr.setstate(std::ios_base::goodbit) : _Ostr.setstate(std::ios_base::eofbit);
        return _Ostr;
    }
    inline std::basic_ostream<char, std::char_traits<char>> & operator<<(std::basic_ostream<char, std::char_traits<char>>& _Ostr, const core::logger_period & fps)
    {
        if (_Ostr.good())
            fps.ok() ? _Ostr.setstate(std::ios_base::goodbit) : _Ostr.setstate(std::ios_base::eofbit);
        return _Ostr;
    }
}

namespace std
{
    inline std::basic_ostream<char, std::char_traits<char>> & __fixed_op(std::basic_ostream<char, std::char_traits<char>>& _Ostr, const char * _Val)
    {
        return _Ostr << _Val;
    }

    inline std::basic_ostream<char, std::char_traits<char>> & __fixed_op(std::basic_ostream<char, std::char_traits<char>>&& _Ostr, const char * _Val)
    {
        return _Ostr << _Val;
    }

    inline std::basic_ostream<char, std::char_traits<char>> & operator<<(std::basic_ostream<char, std::char_traits<char>>&& _Ostr, const char * _Val)
    {
        if (!_Val)
            return __fixed_op(_Ostr, "");
        else
            return __fixed_op(_Ostr, _Val);
    }

    inline std::basic_ostream<char, std::char_traits<char>> & operator<<(std::basic_ostream<char, std::char_traits<char>>& _Ostr, const char * _Val)
    {
        if (!_Val)
            return __fixed_op(_Ostr, "");
        else
            return __fixed_op(_Ostr, _Val);
    }

    inline std::basic_ostream<char, std::char_traits<char>> & operator<<(std::basic_ostream<char, std::char_traits<char>>&& _Ostr, const wchar_t * _Val)
    {
        if (!_Ostr.good())
            return _Ostr;
        return _Ostr << core::ucs2_u8(_Val);
    }
    inline std::basic_ostream<char, std::char_traits<char>> & operator<<(std::basic_ostream<char, std::char_traits<char>>& _Ostr, const wchar_t * _Val)
    {
        if (!_Ostr.good())
            return _Ostr;
        return _Ostr << core::ucs2_u8(_Val);
    }

    inline std::basic_ostream<char, std::char_traits<char>> & operator<<(std::basic_ostream<char, std::char_traits<char>>&& _Ostr, const std::wstring & _Val)
    {
        if (!_Ostr.good())
            return _Ostr;
        return _Ostr << core::ucs2_u8(_Val);
    }
    inline std::basic_ostream<char, std::char_traits<char>> & operator<<(std::basic_ostream<char, std::char_traits<char>>& _Ostr, const std::wstring & _Val)
    {
        if (!_Ostr.good())
            return _Ostr;
        return _Ostr << core::ucs2_u8(_Val);
    }
    inline std::basic_ostream<char, std::char_traits<char>> & operator<<(std::basic_ostream<char, std::char_traits<char>>&& _Ostr, const std::u16string & _Val)
    {
        if (!_Ostr.good())
            return _Ostr;
        return _Ostr << core::ucs2_u8((const wchar_t *)_Val.c_str(), int32_t(_Val.length()));
    }
    inline std::basic_ostream<char, std::char_traits<char>> & operator<<(std::basic_ostream<char, std::char_traits<char>>& _Ostr, const std::u16string & _Val)
    {
        if (!_Ostr.good())
            return _Ostr;
        return _Ostr << core::ucs2_u8((const wchar_t *)_Val.c_str(), int32_t(_Val.length()));
    }
}

namespace core
{
    enum log_e
    {
        log_dbg = 0,
        log_inf,
        log_act,
        log_war,
        log_err,
    };

    class null_logger_stream
    {
    public:
        null_logger_stream() = default;
        ~null_logger_stream() = default;
        template<typename T>
        null_logger_stream & operator << (T && v) { return *this;}
    };

    class logger_stream : public std::ostream, private std::streambuf
    {
    public:
        logger_stream(class logger & logger, log_e lg);
        logger_stream(const logger_stream & another);
        ~logger_stream();

        void set_lg(log_e lg);

        int overflow(int ch);
        std::streamsize xsputn(const char * s, std::streamsize n);
        int sync();
        std::ostream & os() { return *this; }

    private:
        log_e _lg_curr = log_e::log_dbg;
        log_e _lg = log_e::log_inf;
        class logger & _logger;
        std::stringbuf _buffer;
    };

    class logger
    {
    public:
        logger();
        logger(std::string path, log_e lg);
        logger(logger && another) = default;
        ~logger();

        void set_lg(log_e lg) { _lg = lg; }
        log_e lg() const { return _lg; }
        void set_proxy(void * opaque, std::function<void(void * opaque, uint32_t level, uint64_t time_ms, uint32_t pid, uint32_t tid, const char * data, uint32_t length)> proxy) { _proxy_opaque = opaque;  _proxy = proxy; }
        error_e open(std::string path);
        void close();
        void flush();

		void backup_log(const std::string& path);

		error_e log_to_buffer(uint32_t pid, uint32_t tid, log_e lg, std::string text);

        error_e write(log_e lg, std::string text);

        error_e line(log_e lg, std::string text);
        error_e line(log_e lg, const char * text, int32_t length = -1);

        operator std::fstream & () { return _fs; }

        logger_stream operator[](log_e lg)
        {
            return logger_stream(*this, lg);
        }

    public:
#ifdef _DEBUG
        static logger_stream dbg();
#else
        static null_logger_stream dbg() { return null_logger_stream{}; }
#endif
        static logger_stream inf();
        static logger_stream act();
        static logger_stream war();
        static logger_stream err();

    private:
        void writeThread();

    private:
        std::thread _thread;
        std::mutex _mtx_write;
        std::condition_variable _cond;

        std::atomic<bool> _finish = false;
        std::atomic_bool _console = false;
        std::atomic_bool _debug_output= false;

        log_e _lg = log_e::log_dbg;
        std::string _line_tag = "\r\n";
        std::fstream _fs;
		std::string _log_path;
        void * _proxy_opaque = nullptr;
        std::function<void(void * opaque, uint32_t level, uint64_t time_ms, uint32_t pid, uint32_t tid, const char * data, uint32_t length)> _proxy;
        struct log_item
        {
            log_e lg;
            uint64_t time_ms;
            uint32_t pid;
            uint32_t tid;
            std::string text;
        };
		std::list<log_item> _log_list;
        int64_t _line = 0;
        int64_t _line_curr = -1;
    };

    /**
     * Log lines are written as raw bytes (binary stream). Payload is UTF-8 when you use:
     *   - UTF-8 string literals in source (recommended: MSVC /utf-8), or
     *   - wchar_t / std::wstring via operator<< (converted with ucs2_u8).
     * New empty log files get an EF BB BF BOM so tools like Notepad detect UTF-8.
     *
     * Structured wall-time diagnostics for startup/tick/shader JIT use core::perf::hdr / core::WallSplitTimer
     * (see core/wall_timer.h): lines look like  Perf|<domain>|<event> key=value ...
     * Those inf-level lines are suppressed unless -perfinf is passed (core::perf::ShouldEmitPerfInfLogs).
     */
    class global_logger
    {
    public:
        global_logger();
        global_logger(std::string path, log_e lg);
        ~global_logger();

        static void start(std::string path, log_e lg);
        static void stop();

        static void set_proxy(void * opaque, std::function<void(void * opaque, uint32_t level, uint64_t time_ms, uint32_t pid, uint32_t tid, const char * data, uint32_t length)> proxy);
        static logger & ref();
        static logger & instance();

    private:
        std::shared_ptr<logger> _logger;
    };

   inline logger_stream inf() { return global_logger::ref()[log_inf]; }
   inline logger_stream act() { return global_logger::ref()[log_act]; }
   inline logger_stream war() { return global_logger::ref()[log_war]; }
   inline logger_stream err() { return global_logger::ref()[log_err]; }

   void LOG(core::log_e lg, const wchar_t* Format, ...);
}
