﻿#pragma once

RESUMEF_NS
{
	struct state_base_t
	{
		using _Alloc_char = std::allocator<char>;
	private:
		std::atomic<intptr_t> _count{0};
	public:
		void lock()
		{
			++_count;
		}
		void unlock()
		{
			if (--_count == 0)
			{
				destroy_deallocate();
			}
		}
	protected:
		scheduler_t* _scheduler = nullptr;
		//可能来自协程里的promise产生的，则经过co_await操作后，_coro在初始时不会为nullptr。
		//也可能来自awaitable_t，如果
		//		一、经过co_await操作后，_coro在初始时不会为nullptr。
		//		二、没有co_await操作，直接加入到了调度器里，则_coro在初始时为nullptr。调度器需要特殊处理此种情况。
		coroutine_handle<> _coro;

		virtual ~state_base_t();
	private:
		virtual void destroy_deallocate() = 0;
	public:
		virtual void resume() = 0;
		virtual bool has_handler() const = 0;

		void set_scheduler(scheduler_t* sch)
		{
			_scheduler = sch;
		}
		coroutine_handle<> get_handler() const
		{
			return _coro;
		}
	};
	
	struct state_generator_t : public state_base_t
	{
	private:
		virtual void destroy_deallocate() override;
	public:
		virtual void resume() override;
		virtual bool has_handler() const override;

		bool switch_scheduler_await_suspend(scheduler_t* sch, coroutine_handle<> handler);

		void set_initial_suspend(coroutine_handle<> handler)
		{
			_coro = handler;
		}

		static state_generator_t * _Alloc_state()
		{
			_Alloc_char _Al;
			size_t _Size = sizeof(state_generator_t);
#if RESUMEF_DEBUG_COUNTER
			std::cout << "state_generator_t::alloc, size=" << sizeof(state_generator_t) << std::endl;
#endif
			char* _Ptr = _Al.allocate(_Size);
			return new(_Ptr) state_generator_t();
		}
	};

	struct state_future_t : public state_base_t
	{
		enum struct initor_type : uint8_t
		{
			None,
			Initial,
			Final
		};
		//typedef std::recursive_mutex lock_type;
		typedef spinlock lock_type;
	protected:
		mutable lock_type _mtx;
		coroutine_handle<> _initor;
		state_future_t* _parent = nullptr;
#if RESUMEF_DEBUG_COUNTER
		intptr_t _id;
#endif
		std::exception_ptr _exception;
		uint32_t _alloc_size = 0;
		std::atomic<bool> _has_value{ false };
		bool _is_awaitor;
		initor_type _is_initor = initor_type::None;
	public:
		state_future_t()
		{
#if RESUMEF_DEBUG_COUNTER
			_id = ++g_resumef_state_id;
#endif
			_is_awaitor = false;
		}
		explicit state_future_t(bool awaitor)
		{
#if RESUMEF_DEBUG_COUNTER
			_id = ++g_resumef_state_id;
#endif
			_is_awaitor = awaitor;
		}

		virtual void destroy_deallocate() override;
		virtual void resume() override;
		virtual bool has_handler() const override;
	
		inline bool is_ready() const
		{
			return _exception != nullptr || _has_value.load(std::memory_order_acquire) || !_is_awaitor;
		}
		inline bool has_handler_skip_lock() const
		{
			return (bool)_coro || _is_initor != initor_type::None;
		}

		inline scheduler_t* get_scheduler() const
		{
			return _parent ? _parent->get_scheduler() : _scheduler;
		}

		inline state_base_t * get_parent() const
		{
			return _parent;
		}
		inline uint32_t get_alloc_size() const
		{
			return _alloc_size;
		}

		void set_exception(std::exception_ptr e);

		template<class _Exp>
		inline void throw_exception(_Exp e)
		{
			set_exception(std::make_exception_ptr(std::move(e)));
		}

		inline bool future_await_ready()
		{
			//scoped_lock<lock_type> __guard(this->_mtx);
			return _has_value.load(std::memory_order_acquire);
		}
		template<class _PromiseT, typename = std::enable_if_t<is_promise_v<_PromiseT>>>
		void future_await_suspend(coroutine_handle<_PromiseT> handler);

		bool switch_scheduler_await_suspend(scheduler_t* sch, coroutine_handle<> handler);

		template<class _PromiseT, typename = std::enable_if_t<is_promise_v<_PromiseT>>>
		void promise_initial_suspend(coroutine_handle<_PromiseT> handler);
		template<class _PromiseT, typename = std::enable_if_t<is_promise_v<_PromiseT>>>
		void promise_final_suspend(coroutine_handle<_PromiseT> handler);

		template<class _Sty>
		static inline _Sty* _Alloc_state(bool awaitor)
		{
			_Alloc_char _Al;
			size_t _Size = sizeof(_Sty);
#if RESUMEF_DEBUG_COUNTER
			std::cout << "state_future_t::alloc, size=" << _Size << std::endl;
#endif
			char* _Ptr = _Al.allocate(_Size);
			return new(_Ptr) _Sty(awaitor);
		}
	};

	template <typename _Ty>
	struct state_t final : public state_future_t
	{
		friend state_future_t;

		using state_future_t::lock_type;
		using value_type = _Ty;

		explicit state_t(size_t alloc_size) :state_future_t()
		{
			_alloc_size = static_cast<uint32_t>(alloc_size);
		}
		explicit state_t(bool awaitor) :state_future_t(awaitor)
		{
			_alloc_size = sizeof(*this);
		}
	public:
		~state_t()
		{
			if (_has_value.load(std::memory_order_acquire))
				cast_value_ptr()->~value_type();
		}

		auto future_await_resume() -> value_type;
		template<class _PromiseT, typename U, typename = std::enable_if_t<is_promise_v<_PromiseT>>>
		void promise_yield_value(_PromiseT* promise, U&& val);

		template<typename U>
		void set_value(U&& val);
	private:
		value_type * cast_value_ptr()
		{
			return static_cast<value_type*>(static_cast<void*>(_value));
		}

		alignas(value_type) unsigned char _value[sizeof(value_type)] = {0};
	};

	template<>
	struct state_t<void> final : public state_future_t
	{
		friend state_future_t;
		using state_future_t::lock_type;

		explicit state_t(size_t alloc_size) :state_future_t()
		{
			_alloc_size = static_cast<uint32_t>(alloc_size);
		}
		explicit state_t(bool awaitor) :state_future_t(awaitor)
		{
			_alloc_size = sizeof(*this);
		}
	public:
		void future_await_resume();
		template<class _PromiseT, typename = std::enable_if_t<is_promise_v<_PromiseT>>>
		void promise_yield_value(_PromiseT* promise);

		void set_value();
	};
}

