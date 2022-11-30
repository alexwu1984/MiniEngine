#pragma once

#include "core/inc.h"
#include "core/event.h"
#include "core/system.h"

namespace win32
{
    class tpp_worker : public std::enable_shared_from_this<tpp_worker>
    {
    protected:
        virtual ~tpp_worker() {}

    public:
        tpp_worker * ref() { addref(); return this; }
        uint32_t addref() { return ++_ref; }
        uint32_t release()
        {
            if (!_ref || _ref > 0x7ffffffff)
                throw std::exception("invalid reference count");
            uint32_t iref = --_ref;
            if (!iref)
                delete this;
            return iref;
        }
        uint32_t refcount() const { return _ref; }

        virtual void dispatch(core::error_e state)
        {
            done(state);
        }

		virtual void stop_thread()
		{
		}

    protected:
        std::atomic_uint32_t _ref = 1;

    public:
        core::event<void(core::error_e err)> done;

    public:
        static void deleter(tpp_worker * p)
        {
            if (p)
            {
                p->release();
            }
        }
    };

    class tpp_pooler
    {
    public:
        tpp_pooler(std::shared_ptr<tpp_worker> worker) : _worker(worker) {}

        void dispatch(void * instance, core::error_e state)
        {
            _instance = instance;
            _dispatching = core::thread_id();
            {
                std::shared_ptr<tpp_worker> worker = _worker.lock();
                if (worker)
                    worker->dispatch(state);
            }
            _dispatching = 0;
            _instance = nullptr;
        }

        void disassociate();

    protected:
        std::weak_ptr <tpp_worker> _worker;
        std::atomic<void *> _instance = nullptr;
        std::atomic_uint32_t _dispatching = 0;
    };

    class tpp_waiter : public tpp_worker
    {
    public:
        virtual void wait(std::shared_ptr<void> object) = 0;
    };

    class tpp_ovlp : public tpp_worker
    {
    public:
        virtual void wait() = 0;
        virtual void * ovlp() = 0;
    };

    class tpp_iocp: public tpp_worker
    {
    public:
        virtual void wait(std::shared_ptr<void> handle) = 0;
        virtual void * ovlp() = 0;
        virtual void * handle() = 0;

    public:
        core::event<void(core::error_e err, void * ovlp)> iocp_done;
    };

    class tpp_timer: public tpp_worker
    {
    public:
        //utc_clock c++20
        virtual void start(std::chrono::milliseconds period) = 0;
        virtual void start(std::chrono::system_clock::time_point time, std::chrono::milliseconds period) = 0;
        virtual void stop() = 0;
        virtual bool ticking() const = 0;
        virtual std::chrono::milliseconds period() const = 0;
    };

    // 所有等待对象的析构都会等待回调执行完毕，如果这导致了死锁发生，说明是程序设计的失误，不应该在回调里占用主逻辑的临界区
    class thread_pool
    {
    public:
        thread_pool();
        ~thread_pool();

        core::error_e init();
        core::error_e uninit();

        std::shared_ptr<tpp_waiter> create_waiter();
        std::shared_ptr<tpp_ovlp> create_ovlp();
        std::shared_ptr<tpp_iocp> create_iocp();
        std::shared_ptr<tpp_timer> create_timer();

        void * env() const { return _env.get(); }
    private:
        std::shared_ptr<void> _env;
        std::shared_ptr<void> _pool;
        std::shared_ptr<void> _cleangroup;

    public:
        static thread_pool & instance();
    };

}
