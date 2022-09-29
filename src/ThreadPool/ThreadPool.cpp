#include "ThreadPool.h"
#include <cassert>

ThreadPool::ThreadPool(size_t threadCount) : ThreadCount(threadCount)
{
	mThreads.reserve(ThreadCount);
	while (mThreads.size() < ThreadCount) {
		auto taskProcess = [&](std::stop_token stopCondition) {
			while (threadDoTask(stopCondition)) {
			}
		};
		mThreads.emplace_back(taskProcess);
	};
}

ThreadPool::~ThreadPool()
{
	for (auto& thread : mThreads) {
		thread.request_stop();
	}
	mTaskCondition.notify_all();
	mThreads.clear();
}

void ThreadPool::PushTask(Task& task)
{
	{
		std::scoped_lock<std::mutex> lock(mTaskMutex);
		mTaskQueue.push(&task);
	}
	mTaskCondition.notify_one();
}

void ThreadPool::PushTasks(std::vector<Task>::iterator first, std::vector<Task>::iterator last)
{
	{
		std::scoped_lock<std::mutex> lock(mTaskMutex);
		for (auto it = first; it != last; ++it) {
			mTaskQueue.push(&*it);
		}
	}
	mTaskCondition.notify_all();
}

bool ThreadPool::TryHelpOneTask()
{
	Task* task;
	{
		std::unique_lock<std::mutex> lock(mTaskMutex);
		if (mTaskQueue.empty()) {
			return false;
		}
		task = mTaskQueue.front();
		mTaskQueue.pop();
	}
	(*task)();
	return true;
}

bool ThreadPool::threadDoTask(std::stop_token exitThreadCondition)
{
	Task* task;
	{
		std::unique_lock<std::mutex> lock(mTaskMutex);
		mTaskCondition.wait(lock, [&]() { return exitThreadCondition.stop_requested() || !mTaskQueue.empty(); });

		if (exitThreadCondition.stop_requested() && mTaskQueue.empty()) {
			return false;
		}

		task = mTaskQueue.front();
		mTaskQueue.pop();
	}
	(*task)();
	return true;
}
