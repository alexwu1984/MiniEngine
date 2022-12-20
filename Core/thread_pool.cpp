#include "win/tpp_thread_pool.h"
#include "win/win32.h"
#include "core/logger.h"

namespace win32
{
    void tpp_pooler::disassociate()
    {
        void * instance = _instance.exchange(nullptr);
        if (instance && _dispatching == core::thread_id())
        {
            DisassociateCurrentThreadFromCallback((PTP_CALLBACK_INSTANCE)instance);
        }
    }

    tpp_thread_pool::tpp_thread_pool()
    {
    }

    tpp_thread_pool::~tpp_thread_pool()
    {
        uninit();
    }

    core::error_e tpp_thread_pool::init()
    {
        // 在 windows7 以下，在 dll 中自定义线程池，如果 dll 没有手动卸载，这些销毁 API 都会抛异常，干脆就用默认的线程池了
        //auto pfnClean = [](PVOID ObjectContext, PVOID CleanupContext)
        //{
        //    ObjectContext = nullptr;
        //    CleanupContext = nullptr;
        //};
        //PTP_CALLBACK_ENVIRON  pcbenv = new TP_CALLBACK_ENVIRON;
        //PTP_POOL ppool = CreateThreadpool(nullptr);
        //PTP_CLEANUP_GROUP pcleangroup = CreateThreadpoolCleanupGroup();

        //InitializeThreadpoolEnvironment(pcbenv);
        //SetThreadpoolCallbackPool(pcbenv, ppool);
        //SetThreadpoolCallbackCleanupGroup(pcbenv, pcleangroup, pfnClean);

        //_env.reset(pcbenv, [](PTP_CALLBACK_ENVIRON p) { DestroyThreadpoolEnvironment(p); delete p; });
        //_pool.reset(ppool, [](PTP_POOL p) { CloseThreadpool(p); });
        //_cleangroup.reset(pcleangroup, [](PTP_CLEANUP_GROUP p) { CloseThreadpoolCleanupGroup(p); });

        //_env.reset(pcbenv, [](void * p) { delete p; });
        //_pool.reset(ppool, [](void *) {});
        //_cleangroup.reset(pcleangroup, [](void *) {});

        return core::error_ok;
    }

    core::error_e tpp_thread_pool::uninit()
    {
        //if (_env)
        //{
        //    CloseThreadpoolCleanupGroupMembers((PTP_CLEANUP_GROUP)_cleangroup.get(), true, this);
        //    _cleangroup.reset();
        //    _pool.reset();
        //    _env.reset();
        //}
        return core::error_ok;
    }

    std::shared_ptr<tpp_waiter> tpp_thread_pool::create_waiter()
    {
        class tpp_waiter_impl : public tpp_waiter
        {
        public:
            tpp_waiter_impl() {}
            ~tpp_waiter_impl() {}

            void wait(std::shared_ptr<void> object)
            {
                if(!_wait)
                {
                    auto pfnWait = [](PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult)
                    {
                        tpp_pooler * pthis = (tpp_pooler *)Context;
                        core::error_e state = core::error_ok;
                        switch (WaitResult)
                        {
                        case WAIT_OBJECT_0:
                        case WAIT_ABANDONED:
                            state = core::error_ok;
                            break;
                        case WAIT_TIMEOUT:
                            state = core::error_timeout;
                            break;
                        case WAIT_FAILED:
                            state = win32::winerr();
                            break;
                        default:
                            state = core::error_generic;
                        }
                        pthis->dispatch(Instance, state);
                    };

                    tpp_pooler * pooler = new tpp_pooler(shared_from_this());
                    _wait.reset(CreateThreadpoolWait(pfnWait, pooler, NULL), [this, pooler](TP_WAIT * pwait)
                    {
                        if (!pwait)
                            return;

                        pooler->disassociate();
                        WaitForThreadpoolWaitCallbacks(pwait, true);
                        SetThreadpoolWait(pwait, NULL, NULL);
                        CloseThreadpoolWait(pwait);
                        delete pooler;
                    });
                }

                SetThreadpoolWait(_wait.get(), object.get(), NULL);
            }

        private:
            std::shared_ptr<TP_WAIT> _wait;
        };

        std::shared_ptr<tpp_waiter_impl> waiter(new tpp_waiter_impl(), tpp_worker::deleter);
        return waiter;
    }

    std::shared_ptr<tpp_ovlp> tpp_thread_pool::create_ovlp()
    {
        class tpp_ovlp_impl : public tpp_ovlp
        {
        public:
            tpp_ovlp_impl()
            {
                _ovlp->hEvent = CreateEventW(NULL, false, false, NULL);
            }
            ~tpp_ovlp_impl()
            {
                if(_ovlp && _ovlp->hEvent)
                    CloseHandle(_ovlp->hEvent);
            }

            void * ovlp()
            {
                return _ovlp.get();
            }

            void wait()
            {
                if(!_wait)
                {
                    auto pfnWait = [](PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult)
                    {
                        tpp_pooler * pthis = (tpp_pooler *)Context;
                        core::error_e state = core::error_ok;
                        switch (WaitResult)
                        {
                        case WAIT_OBJECT_0:
                        case WAIT_ABANDONED:
                            state = core::error_ok;
                            break;
                        case WAIT_TIMEOUT:
                            state = core::error_timeout;
                            break;
                        case WAIT_FAILED:
                            state = win32::winerr();
                            break;
                        default:
                            state = core::error_generic;
                        }
                        pthis->dispatch(Instance, state);
                    };

                    tpp_pooler * pooler = new tpp_pooler(shared_from_this());
                    _wait.reset(CreateThreadpoolWait(pfnWait, pooler, NULL), [this, pooler](TP_WAIT * pwait)
                    {
                        if (!pwait)
                            return;

                        pooler->disassociate();
                        WaitForThreadpoolWaitCallbacks(pwait, true);
                        SetThreadpoolWait(pwait, NULL, NULL);
                        CloseThreadpoolWait(pwait);
                        delete pooler;
                    });
                }

                SetThreadpoolWait(_wait.get(), _ovlp->hEvent, NULL);
            }

        private:
            std::shared_ptr<OVERLAPPED> _ovlp = std::make_shared<OVERLAPPED>();
            std::shared_ptr<TP_WAIT> _wait;
        };

        std::shared_ptr<tpp_ovlp_impl> ovlp(new tpp_ovlp_impl(), tpp_worker::deleter);
        return ovlp;
    }

    std::shared_ptr<tpp_iocp> tpp_thread_pool::create_iocp()
    {
        class tpp_iocp_impl : public tpp_iocp
        {
        public:
            tpp_iocp_impl() {}
            ~tpp_iocp_impl() {}

            void * ovlp()
            {
                return _ovlp.get();
            }

            void * handle()
            {
                return _handle;
            }

			void wait(std::shared_ptr<void> handle)
			{
                if(!_io || (_handle != handle.get()))
				{
					_handle = nullptr;

					class tpp_pooler_iocp
					{
					public:
						tpp_pooler_iocp(std::shared_ptr<tpp_worker> worker) : _worker(worker) {}

						void dispatch(void * instance, core::error_e state, void * ovlp)
						{
							_instance = instance;
							_dispatching = core::thread_id();
							{
								std::shared_ptr<tpp_worker> worker = _worker.lock();
								if (worker)
								{
									tpp_iocp_impl * iocp_impl = (tpp_iocp_impl *)worker.get();
									std::lock_guard<std::recursive_mutex> locker(iocp_impl->_mtx);
									if (!(worker.use_count() == 1))
									{
										iocp_impl->iocp_done(state, ovlp);
									}
								}
							}
							_dispatching = 0;
							_instance = nullptr;
						}

						void disassociate()
						{
							void * instance = _instance.exchange(nullptr);
							if (instance && _dispatching == core::thread_id())
							{
								DisassociateCurrentThreadFromCallback((PTP_CALLBACK_INSTANCE)instance);
							}
						}
					protected:
						std::weak_ptr<tpp_worker> _worker;
						std::atomic<void *> _instance = nullptr;
						std::atomic_uint32_t _dispatching = 0;
					};

					auto pfnIocp = [](PTP_CALLBACK_INSTANCE Instance, PVOID Context, PVOID Overlapped, ULONG IoResult, ULONG_PTR NumberOfBytesTransferred, PTP_IO Io)
					{
						tpp_pooler_iocp * pthis = (tpp_pooler_iocp *)Context;
						pthis->dispatch(Instance, win32::winerr_val(IoResult), Overlapped);
					};

					tpp_pooler_iocp * pooler = new tpp_pooler_iocp(shared_from_this());
					_io.reset(CreateThreadpoolIo(handle.get(), pfnIocp, pooler, NULL), [this, pooler](TP_IO * pio)
					{
						if (!pio)
							return;

						pooler->disassociate();
						//WaitForThreadpoolIoCallbacks(pio, true);
						CloseThreadpoolIo(pio);
						delete pooler;
					});
					_handle = handle.get();
				}

				if (_io)
				{
					StartThreadpoolIo(_io.get());
				}
			}

			void stop_thread() override
			{
				std::lock_guard<std::recursive_mutex> locker(_mtx);
				_io.reset();
			}

        private:
			std::recursive_mutex _mtx;
            std::shared_ptr<TP_IO> _io;
            std::shared_ptr<OVERLAPPED> _ovlp = std::make_shared<OVERLAPPED>();
            void * _handle = nullptr;
        };

        std::shared_ptr<tpp_iocp_impl> iocp(new tpp_iocp_impl(), tpp_worker::deleter);
        return iocp;
    }

    std::shared_ptr<tpp_timer> tpp_thread_pool::create_timer()
    {
        class tpp_timer_impl : public tpp_timer
        {
        public:
            tpp_timer_impl() {}
            ~tpp_timer_impl() {}

            void dispatch(core::error_e state) override
            {
				std::lock_guard<std::recursive_mutex> locker(_mtx);

                done(state);
                if (_period > 0ms)
                {
                    auto time = std::chrono::system_clock::now();
                    int64_t microseconds = std::chrono::duration_cast<std::chrono::microseconds>(time.time_since_epoch()).count() + _period.count() * 1000;
                    int64_t datatime = microseconds * 10 + 116444736000000000;
                    FILETIME ftime = {};
                    ftime.dwLowDateTime = datatime & 0xffffffff;
                    ftime.dwHighDateTime = datatime >> 32;

                    SetThreadpoolTimer(_timer.get(), &ftime, 0, 0);
                }
            }

            void start(std::chrono::milliseconds period)
            {
                start(std::chrono::system_clock::now(), period);
            }

            void start(std::chrono::system_clock::time_point time, std::chrono::milliseconds period)
            {
                if (_period > 0ms)
                    return;

				std::lock_guard<std::recursive_mutex> locker(_mtx);

                if (!_timer)
                {
                    auto pfnTimer = [](PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER Timer)
                    {
                        tpp_pooler * pthis = (tpp_pooler *)Context;
                        pthis->dispatch(Instance, core::error_ok);
                    };

                    tpp_pooler * pooler = new tpp_pooler(shared_from_this());
                    _timer.reset(CreateThreadpoolTimer(pfnTimer, pooler, NULL), [this, pooler](TP_TIMER * ptimer)
                    {
                        pooler->disassociate();
                        WaitForThreadpoolTimerCallbacks(ptimer, true);
                        SetThreadpoolTimer(ptimer, NULL, 0, 0);
                        CloseThreadpoolTimer(ptimer);
                        delete pooler;
                    });
                }
                int64_t microseconds = std::chrono::duration_cast<std::chrono::microseconds>(time.time_since_epoch()).count() + period.count() * 1000;
                int64_t datatime =  microseconds * 10 + 116444736000000000;
                FILETIME ftime = {};
                ftime.dwLowDateTime = datatime & 0xffffffff;
                ftime.dwHighDateTime = datatime >> 32;

                _period = period;
                SetThreadpoolTimer(_timer.get(), &ftime, 0, 0);
            }

            void stop()
            {
            }

			void stop_thread() override
			{
				std::lock_guard<std::recursive_mutex> locker(_mtx);
				_timer.reset();
			}

            bool ticking() const
            {
                if (!_timer)
                    return false;
                return !!IsThreadpoolTimerSet(_timer.get());
            }

            std::chrono::milliseconds period() const
            {
                return _period;
            }

        private:
			std::recursive_mutex      _mtx;
            std::shared_ptr<TP_TIMER> _timer;
            std::chrono::milliseconds _period = 0ms;
        };

        std::shared_ptr<tpp_timer_impl> timer(new tpp_timer_impl(), tpp_worker::deleter);
        return timer;
    }

    tpp_thread_pool & tpp_thread_pool::instance()
    {
        static tpp_thread_pool __pool;
        static std::once_flag __pool_once_flag;
        std::call_once(__pool_once_flag, []() {__pool.init(); });
        return __pool;
    }
}
#ifdef WINDOWS_PLATFORM_TYPES_GUARD
#include "Windows/HideWindowsPlatformTypes.h"
#endif