#pragma once

#include "core/inc.h"

namespace std
{
	template<class _Ret,
		size_t... _Ix,
		class _Cv_FD,
		class _Cv_tuple_TiD,
		class _Untuple> inline
		auto _Weak_Call_binder(_Invoker_ret<_Ret>, index_sequence<_Ix...>,
			_Cv_FD& _Obj, _Cv_tuple_TiD& _Tpl, _Untuple&& _Ut)
		-> decltype(_Invoker_ret<_Ret>::_Call(_Obj, _Fix_arg(_STD get<_Ix>(_Tpl), _STD move(_Ut))...))
	{	// bind() and bind<R>() invocation
		return (_Invoker_ret<_Ret>::_Call(_Obj, _Fix_arg(_STD get<_Ix>(_Tpl), _STD move(_Ut))...));
	}

	template<class _Ret, class _Fx, class _Tx, class... _Types>
	class _WeakBinder : public _Binder_result_type<_Ret, _Fx>::type
	{
		// wrap bound callable object and arguments
	private:
		typedef std::shared_ptr<_Tx> _ShptrT;
		typedef std::weak_ptr<_Tx> _WeakPtrT;
		typedef std::index_sequence_for<_ShptrT, _Types...> _Seq;
		typedef std::decay_t<_Fx> _First;
		typedef std::tuple<_ShptrT, std::decay_t<_Types>...> _Second;

		_WeakPtrT _MyWptr;
		std::_Compressed_pair<_First, _Second> _Mypair;

	public:
		explicit _WeakBinder(_Fx && _Func, _ShptrT _Shp, _Types &&... _Args)
			: _MyWptr(_Shp), _Mypair(std::_One_then_variadic_args_t(), std::forward<_Fx>(_Func), _ShptrT(), std::forward<_Types>(_Args)...)
		{
		}


		//template<class... _Unbound>
		//auto operator()(_Unbound &&... _Unbargs) -> decltype(std::_Weak_Call_binder(std::_Invoker_ret<_Ret>(), _Seq(), _Mypair._Get_first(), _Mypair._Get_second(),
		//	std::forward_as_tuple(std::forward<_Unbound>(_Unbargs)...)))
		//{
		//	auto & _second = _Mypair._Get_second();
		//	std::get<0>(_second) = _MyWptr.lock();
		//		return (std::_Weak_Call_binder(std::_Invoker_ret<_Ret>(), _Seq(), _Mypair._Get_first(), _Mypair._Get_second(), std::forward_as_tuple(std::forward<_Unbound>(_Unbargs)...)));
		//}

		// 不处理任何返回值
		template<class... _Unbound>
		void operator()(_Unbound &&... _Unbargs)
		{
			auto & _second = _Mypair._Get_second();
			std::get<0>(_second) = _MyWptr.lock();
			if(std::get<0>(_second))
			{
				std::_Weak_Call_binder(std::_Invoker_ret<_Ret>(), _Seq(), _Mypair._Get_first(), _Mypair._Get_second(), std::forward_as_tuple(std::forward<_Unbound>(_Unbargs)...));
				std::get<0>(_second).reset();
			}
		}
	};

	template<class _Fx, class _Tx, class... _Types>
	inline _WeakBinder<std::_Unforced, _Fx, _Tx, _Types...>
		weak_bind(_Fx && _Func, std::shared_ptr<_Tx> _Shp, _Types &&... _Args)
	{
		return (_WeakBinder<std::_Unforced, _Fx, _Tx, _Types...>(std::forward<_Fx>(_Func), _Shp, std::forward<_Types>(_Args)...));
	}
}

namespace core
{
    class event_empty_mutex
    {
    public:
        event_empty_mutex() = default;
        void lock() {}
        void try_lock() {}
        void unlock() {}
    };

    template<typename _Fty, typename _Mutex = event_empty_mutex>
    class event final
    {
        enum flags
        {
            none = 0,
            expired = 0x0001,
        };

        struct callback
        {
            std::function<_Fty> function;
            int32_t flag = flags::none;
			uint64_t id = 0u;
			bool disposable = false;
        };

    public:
        
		template <typename IDType>
		void bind(std::function<_Fty> func, IDType id)
		{
			std::lock_guard<_Mutex> l(_mutex);
			callback c;
			c.function = std::move(func);
			c.id = uint64_t(id);
			_functions.push_back(std::move(c));
		}

		void bind_disposable(std::function<_Fty> func)
		{
			std::lock_guard<_Mutex> l(_mutex);
			callback c;
			c.function = std::move(func);
			c.disposable = true;
			_functions.push_back(std::move(c));
		}

		template <typename IDType>
		void unbind(IDType id)
		{
			std::lock_guard<_Mutex> l(_mutex);
			for (auto&& c : _functions)
			{
				if (c.id == uint64_t(id))
				{
					c.flag |= flags::expired;
				}
			}
		}
		
		void operator += (std::function<_Fty> func)
		{
			bind(std::move(func), 0);
		}

		// 这个函数有BUG，勿用
        template<typename T>
        void operator-=(const T & fun)
        {
            std::lock_guard<_Mutex> l(_mutex);
            std::function<_Fty> func = fun;
            auto addr = func.target<T>();
            if (!addr)
                return;

            for(auto & c : _functions)
            {
                auto & t = c.function.target_type();
                if (t != typeid(T))
                    continue;

                auto a0 = c.function.target<T>();
                if (!a0)
                    continue;

                if(std::memcmp(a0, addr, sizeof(T)) != 0)
                    continue;

                c.flag |= flags::expired;
            };
        }

        template<typename ...ArgsT>
        void operator ()(const ArgsT & ...args)
        {
            std::lock_guard<_Mutex> l(_mutex);
            for (auto & c : _functions)
            {
                if(c.flag & flags::expired)
                    continue;

				if(c.function)
				   c.function(args...);

				if (c.disposable)
					c.flag |= flags::expired;
            }

            _functions.erase(std::remove_if(std::begin(_functions), std::end(_functions), [](const auto & c) { return !!(c.flag & flags::expired); }), _functions.end());
        }

		void unbindall()
		{
			std::lock_guard<_Mutex> l(_mutex);
			_functions.clear();
		}
    public:
        _Mutex _mutex;
        std::list<callback> _functions;
    };
}
