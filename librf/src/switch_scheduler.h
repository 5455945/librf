#pragma once

RESUMEF_NS
{
	struct switch_scheduler_awaitor
	{
		using value_type = void;
		using state_type = state_t<value_type>;
		using promise_type = promise_t<value_type>;
		using lock_type = typename state_type::lock_type;

		switch_scheduler_awaitor(scheduler_t* sch)
			:_scheduler(sch) {}
		switch_scheduler_awaitor(const switch_scheduler_awaitor&) = default;
		switch_scheduler_awaitor(switch_scheduler_awaitor&&) = default;

		switch_scheduler_awaitor& operator = (const switch_scheduler_awaitor&) = default;
		switch_scheduler_awaitor& operator = (switch_scheduler_awaitor&&) = default;

		bool await_ready() noexcept
		{
			return false;
		}

		template<class _PromiseT, typename = std::enable_if_t<traits::is_promise_v<_PromiseT>>>
		bool await_suspend(coroutine_handle<_PromiseT> handler)
		{
			_PromiseT& promise = handler.promise();
			auto* sptr = promise.get_state();
			if (sptr->switch_scheduler_await_suspend(_scheduler))
			{
				counted_ptr<state_t<void>> _state = state_future_t::_Alloc_state<state_type>(true);
				_state->set_value();
				_state->future_await_suspend(handler);

				return true;
			}
			return false;
		}

		void await_resume() noexcept
		{
		}
	private:
		scheduler_t* _scheduler;
	};

	//由于跟when_all/when_any混用的时候，在clang上编译失败：
	//clang把scheduler_t判断成一个is_awaitable，且放弃选择when_all/any(scheduler_t& sch, ...)版本
	//故放弃这种用法
	//inline switch_scheduler_awaitor operator co_await(scheduler_t& sch) noexcept
	//{
	//	return { &sch };
	//}
	inline switch_scheduler_awaitor via(scheduler_t& sch) noexcept
	{
		return { &sch };
	}
	inline switch_scheduler_awaitor via(scheduler_t* sch) noexcept
	{
		return { sch };
	}

}
