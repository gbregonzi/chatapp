#pragma once
#include <memory>
#include <utility>
#include <functional>
using namespace std;

class functionWrapper {
private:
	struct implBase {
		virtual void call() = 0;
		virtual ~implBase() {}
	};

	template<typename F>
	struct implType : implBase
	{
		F f;
		implType(F&& f_) : f(move(f_)) {}
		void call() { f(); }
	};

	unique_ptr<implBase> impl;

public:
	template<typename F>
	functionWrapper(F&& f) : impl(new implType<F>(move(f))){
	};
	
	void operator()();

	functionWrapper();
	
	functionWrapper(functionWrapper&& other) noexcept;

	functionWrapper& operator=(functionWrapper&& other) noexcept;

	functionWrapper(const functionWrapper&) = delete; // Disable copy constructor
	functionWrapper& operator=(const functionWrapper&) = delete; // Disable copy assignment operator
};
