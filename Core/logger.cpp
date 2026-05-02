#include "core/logger.h"
#include <sys/timeb.h>
#include "core/system.h"
#include "win/win32.h"
#include <varargs.h>

#define LOG_FILE_MAX_SIZE 1024*1024*20 // 20MB
namespace core
{
    bool logger_period::ok() const
    {
        static std::map<std::tuple<const char *, int>, uint64_t> __last;
        std::mutex __mtx;
        auto now = std::chrono::system_clock::now();
        auto nowms = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());

        std::lock_guard<std::mutex> l(__mtx);
        uint64_t last = __last[_t];
        if (nowms < last)
            return false;

        if ((nowms - last) * _count < static_cast<uint64_t>(_period.count()))
            return false;

        __last[_t] = nowms;
        return true;
    }

    logger_stream::logger_stream(class logger & logger, log_e lg) :std::ostream(this), _logger(logger)
    {
        if (&_logger != nullptr)
            _lg = _logger.lg();
        set_lg(lg);
    }
    logger_stream::logger_stream(const logger_stream & another) : std::ostream(this), _logger(another._logger)
    {
        if (&_logger != nullptr)
            _lg = _logger.lg();
        set_lg(another._lg);
    }
    logger_stream::~logger_stream()
    {
        if (_buffer.in_avail())
        {
            if(&_logger != nullptr)
            {
                _logger.line(_lg, _buffer.str());
                _logger.flush();
            }
            _buffer.pubseekoff(0, std::ios::beg);
        }
    }

    void logger_stream::set_lg(log_e lg)
    {
        _lg = lg;
        if (&_logger == nullptr)
            return;
        _lg >= _logger.lg() ? setstate(std::ios_base::goodbit) : setstate(std::ios_base::eofbit);
    }

    int logger_stream::overflow(int ch)
    {
        return &_logger != nullptr ? _buffer.sputc(ch) : 0;
    }

    std::streamsize logger_stream::xsputn(const char * s, std::streamsize n)
    {
        return &_logger != nullptr ? _buffer.sputn(s, n) : 0;
    }

    int logger_stream::logger_stream::sync()
    {
        if (&_logger != nullptr)
            _logger.flush();
        return 0;
    }

    logger::logger()
    {
        _thread = std::thread(std::bind(&logger::writeThread, this));
    }

    logger::logger(std::string path, log_e lg) : _lg(lg)
    {
        open(path);
        _thread = std::thread(std::bind(&logger::writeThread, this));
    }

    logger::~logger()
    {
        close();
    }

    error_e logger::open(std::string path)
    {
        if (path.empty())
            return error_args;

		_log_path = path;
        _fs.open(path, std::fstream::binary | std::fstream::out | std::fstream::app);
		_fs.seekg(0, _fs.end);
		long long filesize = _fs.tellg();
		if (filesize > LOG_FILE_MAX_SIZE)
		{
			backup_log(path);
		}

        return _fs.good() ? error_ok : error_io;
    }

	void logger::close()
	{
        if (_thread.joinable())
        {
            _finish = true;
            _cond.notify_all();
            _thread.join();
        }

        if (_fs.good())
        {
            _fs.flush();
            _fs.close();
        }
    }

    void logger::flush()
    {
		std::unique_lock<std::mutex> ul(_mtx_write);
        if (_fs.good())
            _fs.flush();
    }

	static std::string parent_dir_from_path(std::string path)
	{
		const std::string::size_type pos = path.find_last_of("/\\");
		if (pos == std::string::npos)
			return {};
		return path.substr(0, pos + 1);
	}

	static std::string filename_from_path(std::string path)
	{
		const std::string::size_type pos = path.find_last_of("/\\");
		if (pos == std::string::npos)
			return path;
		return path.substr(pos + 1);
	}

	void logger::backup_log(const std::string& path)
	{
		if (_fs.fail())
		{
			return;
		}
		if (_fs.is_open())
		{
			_fs.close();
		}
		const std::string dirPath = parent_dir_from_path(path);
		const std::string baseName = filename_from_path(path);
		const std::string logBkPath = dirPath + baseName + ".bk";
		const std::string logBkBkPath = dirPath + baseName + ".bk.bk";

		std::fstream _logBkFs(logBkPath);
		if (!_logBkFs.is_open())
		{
			::MoveFileW(u8_ucs2(path).c_str(), u8_ucs2(logBkPath).c_str());
		}
		else
		{
			_logBkFs.close();
			std::fstream _logBkBkFs(logBkBkPath);
			if (_logBkBkFs.is_open())
			{
				_logBkBkFs.close();
				DeleteFileW(u8_ucs2(logBkBkPath).c_str());
			}
			::MoveFileW(u8_ucs2(logBkPath).c_str(), u8_ucs2(logBkBkPath).c_str());
			::DeleteFileW(u8_ucs2(logBkPath).c_str());
			::MoveFileW(u8_ucs2(path).c_str(), u8_ucs2(logBkPath).c_str());
		}
		_fs.open(path, std::fstream::binary | std::fstream::out | std::fstream::app);
	}

	error_e logger::log_to_buffer(uint32_t pid, uint32_t tid, log_e lg, std::string text)
	{
		if (_line_curr == _line)
			return error_ok;

        if (!_thread.joinable())
            return error_ok;

		_line_curr = _line;
		if (pid == -1)
			pid = core::process_id();

		if (tid == -1)
			tid = core::thread_id();

		if (lg >= _lg)
		{
            auto now = std::chrono::system_clock::now();
            auto nowms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

            std::lock_guard<std::mutex> lock(_mtx_write);
            _log_list.push_back({ lg, uint64_t(nowms), pid, tid, text });
            _cond.notify_all();
		}
		return error_ok;
	}

    error_e logger::write(log_e lg, std::string text)
    {
		error_e err = log_to_buffer(-1, -1, lg, text);
		++_line;
		return err;
    }

    error_e logger::line(log_e lg, std::string text)
    {
		error_e err = log_to_buffer(-1, -1, lg, text);
        ++_line;
        return err;
    }

    error_e logger::line(log_e lg, const char * text, int32_t length)
    {
		error_e err = log_to_buffer(-1, -1, lg, std::string(text, length));
		++_line;
		return err;
    }

#ifdef _DEBUG
    logger_stream logger::dbg() { return global_logger::ref()[log_dbg]; }
#endif
    logger_stream logger::inf() { return global_logger::ref()[log_inf]; }
    logger_stream logger::act() { return global_logger::ref()[log_act]; }
    logger_stream logger::war() { return global_logger::ref()[log_war]; }
    logger_stream logger::err() { return global_logger::ref()[log_err]; }

    void logger::writeThread()
    {
        std::unique_lock<std::mutex> ul(_mtx_write);
        while (true)
        {
			long long filesize = _fs.tellg();
			if (filesize > LOG_FILE_MAX_SIZE && _log_path.size())
			{
				backup_log(_log_path);
			}
            _cond.wait(ul, [this]() { return _finish || !_log_list.empty(); });
            std::list<log_item> items = std::move(_log_list);
            if (items.empty() && _finish)
                break;

            for(auto & item : items)
            {
                if (_proxy)
                    _proxy(_proxy_opaque, item.lg, item.time_ms, item.pid, item.tid, item.text.data(), (uint32_t)item.text.length());
                else
                {
                    time_t ms = time_t(item.time_ms / 1000);
                    tm tm;
                    localtime_s(&tm, &ms);

                    char temp[64];
                    const char * level[] = { "dbg", "inf", "act", "war", "err" };
                    int32_t nchars = _snprintf_s(temp, 64, "[%04d-%02d-%02d %02d:%02d:%02d %03d][pid:%-6d][tid:%-6d]%s ",
                        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                        tm.tm_hour, tm.tm_min, tm.tm_sec, uint32_t(item.time_ms % 1000),
                        item.pid, item.tid,
                        level[std::clamp((int32_t)item.lg, 0, (int32_t)std::size(level))]);

                    std::string log_text(temp, nchars);
                    log_text += item.text;
                    log_text += _line_tag;

                    if (_console)
                        std::cout << item.text;

                    if (_debug_output)
                        OutputDebugStringW(core::u8_ucs2(log_text).data());

                    _fs.write(log_text.data(), log_text.length());
                }
            }
            _fs.flush();
        }
    }

	static std::shared_ptr<logger> __global_logger;
    static std::mutex __mtx_global_logger;

    global_logger::global_logger()
    {
        std::lock_guard<std::mutex> lock(__mtx_global_logger);
        if(!__global_logger)
            __global_logger = std::make_shared<logger>();
    }

    global_logger::global_logger(std::string path, log_e lg)
    {
        std::cout << std::endl << "log file " << path << std::endl;
        std::lock_guard<std::mutex> lock(__mtx_global_logger);
        if (!__global_logger)
            __global_logger = std::make_shared<logger>(path, lg);
    }

    global_logger::~global_logger()
    {
    }

    void global_logger::start(std::string path, log_e lg)
    {
        //std::cout << std::endl << "log file " << path << std::endl;
        std::lock_guard<std::mutex> lock(__mtx_global_logger);
        if (!__global_logger)
            __global_logger = std::make_shared<logger>(path, lg);
    }

    void global_logger::stop()
    {
        __global_logger.reset();
    }

    void global_logger::set_proxy(void * opaque, std::function<void(void * opaque, uint32_t level, uint64_t time_ms, uint32_t pid, uint32_t tid, const char * data, uint32_t length)> proxy)
    {
        if (__global_logger)
            __global_logger->set_proxy(opaque, proxy);
    }

    logger & global_logger::ref()
    {
        return *__global_logger;
    }

    logger & global_logger::instance()
    {
        if(!__global_logger)
        {
            std::lock_guard<std::mutex> lock(__mtx_global_logger);
            if (!__global_logger)
                __global_logger = std::make_shared<logger>();
        }
        return *__global_logger;
    }

	void LOG(core::log_e lg,const wchar_t* formatstring, ...)
	{
		int nSize = 0;
        wchar_t buff[2048]{0};
		memset(buff, 0, sizeof(buff));
		va_list args;
		va_start(args, formatstring);
		nSize = _vsnwprintf_s(buff, _countof(buff), _TRUNCATE, formatstring, args);
		
        if (lg == log_dbg)
        {
            core::logger::dbg() << buff;
        }
        else if (lg == log_act)
        {
            core::logger::act() << buff;
        }
        else if (lg == log_inf)
        {
            core::logger::inf() << buff;
        }
        else if (lg == log_war)
        {
            core::logger::war() << buff;
        }
        else if (lg == log_err)
        {
            core::logger::err() << buff;
        }

		va_end(args);
	}

}
