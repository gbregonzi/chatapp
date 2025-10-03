
#include <memory>
#include <utility>

class function_wrapper {
	struct impl_base {
		virtual void call() = 0;
		virtual ~impl_base() {}
	};

	template<typename F>
	struct impl_type : impl_base
	{
		F f;
		impl_type(F&& f_) : f(std::move(f_)) {}
		void call() { f(); }
	};

	std::unique_ptr<impl_base> impl;

public:
	template<typename F>
	function_wrapper(F&& f) :
		impl(new impl_type<F>(std::move(f)))
	{
	}

	void operator()() { impl->call(); }

	function_wrapper()
	{
	}

	function_wrapper(function_wrapper&& other) noexcept :
		impl(std::move(other.impl))
	{
	}

	function_wrapper& operator=(function_wrapper&& other) noexcept
	{
		impl = std::move(other.impl);
		return *this;
	}

	function_wrapper(const function_wrapper&) = delete; // Disable copy constructor
	function_wrapper(function_wrapper&) = delete; // Disable copy assignment operator
};
