#pragma once

#include "core/inc.h"

//异步调用，在主线程触发（主线程消息循环调用 MsgWaitForMultipleObjectsEx）
namespace core
{
    class object;

    class invoke_task
    {
    public:
        virtual ~invoke_task() = default;
        virtual core::error_e trigger() = 0;
    };

    template<typename T>
    class invoke_task_promise : public invoke_task
    {
    public:
        invoke_task_promise(std::function<T()> fun) : _fun(fun) {}

        core::error_e trigger()
        {
            if constexpr (std::is_void_v<T>)
            {
                _fun();
                _promise.set_value();
            }
            else
                _promise.set_value(_fun());
            return core::error_ok;
        }

        std::future<T> get_future()
        {
            return _promise.get_future();
        }

        std::promise<T> _promise;
        std::function<T()> _fun;
    };

    class iinvokable
    {
    public:
        virtual ~iinvokable() {}
    };

    class invoke_helper
    {
    public:
        invoke_helper();
        ~invoke_helper();

        invoke_helper & ref();

        uint32_t thread_id() const { return _id; }
        void * thread_handle() const;

        bool can_safe_invoke() const;
        void check_invoke();

        core::error_e add(std::shared_ptr<iinvokable> invoker, std::function<void()>);
        core::error_e add(std::shared_ptr<iinvokable> invoker, std::shared_ptr<invoke_task> task);
        core::error_e trigger();
        core::error_e clear();
    private:
        uint32_t _id = 0;
        mutable void * _thread = nullptr;
        mutable std::mutex _mtx;
        typedef std::map<std::weak_ptr<iinvokable>, std::vector<std::function<void()>>, std::owner_less<std::weak_ptr<iinvokable>>> invoker_map;
        typedef std::map<std::weak_ptr<iinvokable>, std::vector<std::shared_ptr<invoke_task>>, std::owner_less<std::weak_ptr<iinvokable>>> task_map;
        invoker_map _invokers;
        task_map _tasks;
    };

    uint64_t create_objectid();
    invoke_helper & get_invoke_helper();
    void invokable_clear();

    template<typename IT>
    class invokable : public std::enable_shared_from_this<IT>, public iinvokable
    {
    public:
        invokable() = default;
        virtual ~invokable() = default;

        template<typename T>
        std::shared_ptr<T> share_ref()
        {
            return std::dynamic_pointer_cast<T>(std::enable_shared_from_this<IT>::shared_from_this());
        }

        template<typename T>
        std::weak_ptr<T> weak_ref()
        {
            return std::dynamic_pointer_cast<T>(std::enable_shared_from_this<IT>::shared_from_this());
        }

        uint64_t id() const { return _id; }

        bool can_safe_invoke() const
        {
            return _invoke_helper.can_safe_invoke();
        }

        void check_invoke()
        {
            return _invoke_helper.check_invoke();
        }

        bool invoke_expired() const
        {
            return std::enable_shared_from_this<IT>::weak_from_this().expired();
        }

        core::error_e invoke(std::function<void()> fun)
        {
            if (std::enable_shared_from_this<IT>::weak_from_this().expired())
                return core::error_state;
            return _invoke_helper.add(std::enable_shared_from_this<IT>::shared_from_this(), fun);
        }

        template<typename T>
        std::future<T> dispatch(std::function<T()> fun)
        {
            if (std::enable_shared_from_this<IT>::weak_from_this().expired())
                return core::error_state;
            auto task = std::make_shared<invoke_task_promise<T>>(fun);
            _invoke_helper.add(std::enable_shared_from_this<IT>::shared_from_this(), task);
            return task->get_future();
        }

        template<typename FunT>
        decltype(std::declval<FunT>()()) async(FunT fun)
        {
            if (std::enable_shared_from_this<IT>::weak_from_this().expired())
                return {};

            if (can_safe_invoke())
                return fun();

            typedef decltype(std::declval<FunT>()()) T;
            auto task = std::make_shared<invoke_task_promise<T>>(fun);
            _invoke_helper.add(std::enable_shared_from_this<IT>::shared_from_this(), task);
            return task->get_future().get();
        }

        template<typename T>
        bool is_type_of()
        {
            return dynamic_cast<T *>(this) != nullptr;
        }

    protected:
        uint64_t _id = create_objectid();
        invoke_helper & _invoke_helper = get_invoke_helper();
    };
}
