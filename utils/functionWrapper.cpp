
#include <memory>
#include <utility>
#include "functionWrapper.h"
using namespace std;



void functionWrapper::operator()() { 
	impl->call(); 
}

functionWrapper::functionWrapper()
{
}

functionWrapper::functionWrapper(functionWrapper&& other) noexcept{
	impl = move(other.impl);
}

functionWrapper& functionWrapper::operator=(functionWrapper&& other) noexcept{
	impl = move(other.impl);
	return *this;
}


