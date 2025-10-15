#include "threadPool.h"


threadPool::threadPool(vector<thread> &threads) : m_Joiner(threads) {
	int const num_threads = thread::hardware_concurrency();// Get the number of hardware threads available
	try {
		for (size_t i = 0; i < num_threads; ++i) {
			threads.emplace_back(&threadPool::WorkerThread, this);
		}
	}
	catch (...) {
		m_Done.store(true); // Set done to true to stop the worker threads
		throw; // Re-throw the exception to be handled by the caller
	}
}

void threadPool::WorkerThread() {
	while (!m_Done) {
		functionWrapper task;
		if (m_WorkQueue.tryPop(task)) {
			task();
		} else {
			this_thread::yield(); // Yield to avoid busy waiting
		}
	}
}

threadPool::~threadPool() {
	m_Done.store(true); // Signal the worker threads to stop
}

void threadPool::runPendingTasks() {
	functionWrapper task;
	if (m_WorkQueue.tryPop(task)) {
		task(); // Execute the task
	}
	else{
		this_thread::yield(); // Yield to avoid busy waiting
	}	
}
