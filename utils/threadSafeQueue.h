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
	queue<shared_ptr<T>> m_DataQueue;
	condition_variable m_DataCond;

public:
	threadSafeQueue()
	{
	}

	void waitAndPop(T& value)
	{
		unique_lock<mutex> lk(m_Mutex);
		m_DataCond.wait(lk, [this] {
			return !m_DataQueue.empty(); 
		});
		value = move(*m_DataQueue.front());
		m_DataQueue.pop();
	}

	bool tryPop(T& value)
	{
		lock_guard lk(m_Mutex);
		if (m_DataQueue.empty())
			return false;
		value = move(*m_DataQueue.front());
		m_DataQueue.pop();
		return true;
	}

	shared_ptr<T> waitAndPop()
	{
		unique_lock<mutex> lk(m_Mutex);
		m_DataCond.wait(lk, [this] {
			return !m_DataQueue.empty(); 
		});
		shared_ptr<T> res = m_DataQueue.front();
		m_DataQueue.pop();
		return res;
	}

	shared_ptr<T> tryPop()
	{
		lock_guard lk(m_Mutex);
		if (m_DataQueue.empty())
			return shared_ptr<T>();
		shared_ptr<T> res = m_DataQueue.front();
		m_DataQueue.pop();
		return res;
	}

	bool empty() const
	{
		lock_guard lk(m_Mutex);
		return m_DataQueue.empty();
	}

	void push(T new_value)
	{
		shared_ptr<T> data(make_shared<T>(move(new_value)));
		lock_guard lk(m_Mutex);
		m_DataQueue.push(data);
		m_DataCond.notify_one();
	}
}; 

