#include "core/invokable.h"
#include "win/win32.h"
#include "core/logger.h"

namespace core
{
    static thread_local std::shared_ptr<invoke_helper> __helper;
    static std::atomic_uint64_t __object_id = 1;

    void invokable_clear()
    {
        if (__helper)
            __helper->clear();
    }

    uint64_t create_objectid()
    {
        static const uint64_t _one = 1;
        uint64_t val = __object_id.fetch_add(_one);
        //return std::hash_value<uint64_t>(val);
        return val;
    }

    invoke_helper & get_invoke_helper()
    {
        if (!__helper)
            __helper = std::make_shared<invoke_helper>();
        return *__helper;
    }

    static void CALLBACK InvokerAPCCallBack(ULONG_PTR dwParam)
    {
        if(__helper)
            __helper->trigger();
    }

    invoke_helper::invoke_helper()
    {
        _id = GetCurrentThreadId();
    }
    invoke_helper::~invoke_helper()
    {
        std::unique_lock<std::mutex> lock(_mtx);
        if (_thread)
        {
            CloseHandle(_thread);
            _thread = nullptr;
            _id = 0;
        }
        lock.unlock();
    }

    invoke_helper & invoke_helper::ref()
    {
        if (_id != GetCurrentThreadId())
            throw 0;

        return *this;
    }

    bool invoke_helper::can_safe_invoke() const
    {
        return ::GetCurrentThreadId() == _id;
    }

    void invoke_helper::check_invoke()
    {
        if (GetCurrentThreadId() != _id)
            throw error_access;
    }

    void * invoke_helper::thread_handle() const
    {
        if (!_id)
            return nullptr;

        std::lock_guard<std::mutex> lock(_mtx);
        if (!_thread)
        {
            _thread = OpenThread(THREAD_SET_CONTEXT, false, _id);
            if (!_thread)
            {
                core::logger::err() << __FUNCTION__" OpenThread" << win32::winerr_str(GetLastError());
            }
        }
        return _thread;
    }

    core::error_e invoke_helper::add(std::shared_ptr<iinvokable> invoker, std::function<void()> fun)
    {
        if (!_id)
            return error_state;
        std::lock_guard<std::mutex> lock(_mtx);
        if (!_thread)
        {
            _thread = OpenThread(THREAD_SET_CONTEXT, false, _id);
            if (!_thread)
            {
                logger::err() << __FUNCTION__" OpenThread" << win32::winerr_str(GetLastError());
            }
        }
        if (!_thread)
            return error_state;

        _invokers[invoker].push_back(fun);
        ::QueueUserAPC(InvokerAPCCallBack, (HANDLE)_thread, 0);
        return error_ok;
    }

    core::error_e invoke_helper::add(std::shared_ptr<iinvokable> invoker, std::shared_ptr<invoke_task> task)
    {
        if (!_id)
            return error_state;
        std::lock_guard<std::mutex> lock(_mtx);
        if (!_thread)
        {
            _thread = OpenThread(THREAD_SET_CONTEXT, false, _id);
            if (!_thread)
            {
                logger::err() << __FUNCTION__" OpenThread" << win32::winerr_str(GetLastError());
            }
        }
        if (!_thread)
            return error_state;

        _tasks[invoker].push_back(task);
        ::QueueUserAPC(InvokerAPCCallBack, (HANDLE)_thread, 0);
        return error_ok;
    }

    core::error_e invoke_helper::trigger()
    {
        if (_id != GetCurrentThreadId())
            return error_state;

        invoker_map invokers;
        task_map tasks;
        while (true)
        {
            {
                std::lock_guard<std::mutex> lock(_mtx);
                invokers = std::move(_invokers);
                tasks = std::move(_tasks);
            }

            if (invokers.empty() && tasks.empty())
                break;

            for (invoker_map::iterator iter = invokers.begin(); iter != invokers.end(); ++iter)
            {
                auto invoker = iter->first.lock();
                if (!invoker)
                    continue;

                for (auto & fun : iter->second)
                    fun();
            }

            for (task_map::iterator iter = tasks.begin(); iter != tasks.end(); ++iter)
            {
                auto invoker = iter->first.lock();
                if (!invoker)
                    continue;

                for (auto & task : iter->second)
                    task->trigger();
            }

        }
        return error_ok;
    }

    core::error_e invoke_helper::clear()
    {
        _invokers.clear();
        _tasks.clear();
        return error_ok;
    }
}
#ifdef WINDOWS_PLATFORM_TYPES_GUARD
#include "Windows/HideWindowsPlatformTypes.h"
#endif