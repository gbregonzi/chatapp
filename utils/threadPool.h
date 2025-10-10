#pragma once

#include <vector>
#include <thread>
#include <future>   
#include <atomic>
#include <functional>
#include "threadSafeQueue.h"
#include "functionWrapper.h"


using namespace std;

struct joinableThread {
	vector<thread>& m_Threads;

	joinableThread(vector<thread>& _threads) : m_Threads(_threads) {
	}

	~joinableThread() {
		for (auto& t : m_Threads) {
			if (t.joinable()) {
				t.join();
			}
		}
	}
};

class threadPool {
private:
    atomic_bool m_Done{ false };
	threadSafeQueue<functionWrapper> m_WorkQueue; 
	vector<thread> m_Threads;
	joinableThread m_Joiner;

	void WorkerThread();

    public:
	threadPool(vector<thread> threads);
	~threadPool();

	template<typename Function_Type>
	future<invoke_result_t<Function_Type>> submit(Function_Type f) { // Accepts a std::function as a task
		using result_type = invoke_result_t<Function_Type>;
		packaged_task<result_type()> task(move(f)); // Create a packaged task
		future<result_type> res = task.get_future(); // Get the future from the packaged task
		m_WorkQueue.push(move(task)); // Push the task to the work queue
		return res; // Return the future to the caller
	}
	void runPendingTasks();
};
