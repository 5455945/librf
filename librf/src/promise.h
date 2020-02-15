﻿#pragma once
#include "state.h"

namespace resumef
{
	template <typename _Ty>
	struct promise_impl_t
	{
		using value_type = _Ty;
		using state_type = state_t<value_type>;
		using promise_type = promise_t<value_type>;
		using future_type = future_t<value_type>;
		using lock_type = typename state_type::lock_type;

		counted_ptr<state_type> _state = make_counted<state_type>();

		promise_impl_t() {}
		promise_impl_t(promise_impl_t&& _Right) noexcept = default;
		promise_impl_t& operator = (promise_impl_t&& _Right) noexcept = default;
		promise_impl_t(const promise_impl_t&) = delete;
		promise_impl_t & operator = (const promise_impl_t&) = delete;

		auto initial_suspend() noexcept;
		auto final_suspend() noexcept;
		void set_exception(std::exception_ptr e);
		future_type get_return_object();
		void cancellation_requested();
	};
		
	template<class _Ty>
	struct promise_t : public promise_impl_t<_Ty>
	{
		using promise_impl_t<_Ty>::_state;

		void return_value(value_type val);
		void yield_value(value_type val);
	};

	template<>
	struct promise_t<void> : public promise_impl_t<void>
	{
		using promise_impl_t<void>::_state;

		void return_void();
		void yield_value();
	};

}

