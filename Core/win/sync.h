#pragma once

#include "core/inc.h"

namespace win32
{
    class signal
    {
    public:
        signal();
        signal(bool init, bool manual);
        signal(std::string name, bool init, bool manual);
        signal(signal && another) noexcept;
        ~signal();

        core::error_e create(std::string name, bool init, bool manual);
		core::error_e create(bool init, bool manual);
        core::error_e open(std::string name);

        core::error_e wait();
        core::error_e wait_for(std::chrono::milliseconds timeout);
        core::error_e set();
        core::error_e reset();

        void * handle() const { return _handle; }

        signal & operator =(signal && another) noexcept;
    private:
        void * _handle = nullptr;
    };

    class mutex
    {
    public:
        mutex();
        mutex(bool own);
        mutex(std::string name, bool own);
        mutex(mutex && another) noexcept;
        ~mutex();

        core::error_e create(std::string name, bool own);
        core::error_e open(std::string name);

        core::error_e lock();
        core::error_e try_lock(std::chrono::milliseconds timeout);
        core::error_e unlock();

        void * handle() const { return _handle; }

        mutex & operator =(mutex && another) noexcept;
    private:
        void * _handle = nullptr;
    };

	namespace sync
	{
		class sync_object
		{
		public:
			virtual ~sync_object() {}
			virtual void lock() = 0;
			virtual bool try_lock() = 0;
			virtual void unlock() = 0;
		};

		class event
		{
		public:
			event();
			event(bool manual, bool set);

			void set();
			void reset();

			void* handle() const { return _handle; }
		private:
			void* _handle;
		};

		class mutex : public sync_object
		{
		public:
			mutex();
			~mutex();

			void lock();
			bool try_lock();
			void unlock();

			void* handle() const { return _handle; }
		private:
			void* _handle;
		};

		class section : public sync_object
		{
		public:
			section();
			~section();

			void lock();
			bool try_lock() { lock(); return true; }
			void unlock();

			std::shared_ptr<void> data() const { return _cs; }
		private:
			std::shared_ptr<void> _cs;
		};

		template<typename _Lock>
		class locker
		{
		public:
			locker(_Lock& lock) :_lock(lock)
			{
				_lock.lock();
			}
			~locker()
			{
				_lock.unlock();
			}
		private:
			_Lock& _lock;
		};
	}
}
