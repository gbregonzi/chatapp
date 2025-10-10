#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

using namespace std;

template<typename T>
class threadSafeQueue
{
private:
	mutex m_Mutex;
	queue<shared_ptr<T>> dataQueue;
	condition_variable dataCond;

public:
	threadSafeQueue()
	{
	}

	void waitAndPop(T& value)
	{
		unique_lock<mutex> lk(m_Mutex);
		dataCond.wait(lk, [this] {
			return !dataQueue.empty(); 
		});
		value = move(*dataQueue.front());
		dataQueue.pop();
	}

	bool tryPop(T& value)
	{
		lock_guard<mutex> lk(m_Mutex);
		if (dataQueue.empty())
			return false;
		value = move(*dataQueue.front());
		dataQueue.pop();
		return true;
	}

	shared_ptr<T> waitAndPop()
	{
		unique_lock<mutex> lk(m_Mutex);
		dataCond.wait(lk, [this] {
			return !dataQueue.empty(); 
		});
		shared_ptr<T> res = dataQueue.front();
		dataQueue.pop();
		return res;
	}

	shared_ptr<T> tryPop()
	{
		lock_guard<mutex> lk(m_Mutex);
		if (dataQueue.empty())
			return shared_ptr<T>();
		shared_ptr<T> res = dataQueue.front();
		dataQueue.pop();
		return res;
	}

	bool empty() const
	{
		lock_guard<mutex> lk(m_Mutex);
		return dataQueue.empty();
	}

	void push(T new_value)
	{
		shared_ptr<T> data(make_shared<T>(move(new_value)));
		lock_guard<mutex> lk(m_Mutex);
		dataQueue.push(data);
		dataCond.notify_one();
	}
}; 

