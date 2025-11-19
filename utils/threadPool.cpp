#include "threadPool.h"


threadPool_::threadPool_(vector<thread> &threads) : m_Joiner(threads) {
	int const num_threads = thread::hardware_concurrency();// Get the number of hardware threads available
	try {
		for (size_t i = 0; i < num_threads; ++i) {
			threads.emplace_back(&threadPool_::WorkerThread, this);
		}
	}
	catch (...) {
		m_Done.store(true); // Set done to true to stop the worker threads
		throw; // Re-throw the exception to be handled by the caller
	}
}

void threadPool_::WorkerThread() {
	while (!m_Done) {
		functionWrapper task;
		if (m_WorkQueue.tryPop(task)) {
			task();
		} else {
			this_thread::yield(); // Yield to avoid busy waiting
		}
	}
}

threadPool_::~threadPool_() {
	m_Done.store(true); // Signal the worker threads to stop
}

void threadPool_::runPendingTasks() {
	functionWrapper task;
	if (m_WorkQueue.tryPop(task)) {
		task(); // Execute the task
	}
	else{
		this_thread::yield(); // Yield to avoid busy waiting
	}	
}

ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
    for (size_t i = 0; i < numThreads; ++i)
        workers.emplace_back([this] {
            while (true) {
                function<void()> task;
                {
                    unique_lock<mutex> lock(queueMutex);
                    condition.wait(lock, [this] { return stop || !tasks.empty(); });
                    if (stop && tasks.empty()) return;
                    task = move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        });
}

ThreadPool::~ThreadPool() {
	{
		lock_guard<mutex> lock(queueMutex);
		stop = true;
	}
	condition.notify_all();
	for (thread &worker : workers)
		worker.join();
}

void ThreadPool::enqueue(function<void()> task) {
    {
        lock_guard<mutex> lock(queueMutex);
        tasks.push(move(task));
    }
    condition.notify_one();
}
