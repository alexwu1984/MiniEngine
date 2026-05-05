#pragma once

#include "core/inc.h"
#include "core/path.h"

namespace core
{
    core::filesystem::path  temp_path();
    core::filesystem::path  current_path();
    bool current_path(core::filesystem::path path);
    core::filesystem::path  appdata_path();
	std::wstring getwide_appdata_path();
    core::filesystem::path  process_path();
    core::filesystem::path  process_directory();
    core::filesystem::path  module_directory();
    bool create_directory(const std::wstring& path);
    std::string  process_name();
    uint32_t  thread_id();
    uint32_t  process_id();
    void set_game_thread_id(uint32_t id);
    bool is_in_gamethread();
    bool  isFileExist(core::filesystem::path path);

    void  thread_set_name(int thread_id, const char * name);
    enum thread_priority
    {
        thread_priority_idle,
        thread_priority_lowest,
        thread_priority_low,
        thread_priority_normal,
        thread_priority_high,
        thread_priority_highest,
        thread_priority_realtime,
    };
    void  thread_set_priority(thread_priority priority);
	bool save_as_bitmap(const char* pData, uint32_t nLen, int w, int h, int bitCount, const wchar_t* pszFileName);
    bool  create_process(const std::wstring& cmdLine);
    bool ceate_dump(const wchar_t* name_prefix, struct _EXCEPTION_POINTERS* ExceptionInfo);
    void outputDebugString(const wchar_t* pcString, ...);

    /**
     * RAII for CoInitializeEx(nullptr, COINIT_MULTITHREADED) on the current thread.
     * Same role as UE4 Windows startup: initialize COM once at process entry before WIC / etc.
     */
    struct scoped_com_mta_init
    {
        scoped_com_mta_init() noexcept;
        ~scoped_com_mta_init();
        scoped_com_mta_init(const scoped_com_mta_init&) = delete;
        scoped_com_mta_init& operator=(const scoped_com_mta_init&) = delete;

    private:
        bool com_needs_uninit_;
    };
}
