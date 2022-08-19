#include "ThreadPool.h"
#include <cassert>

ThreadPool::ThreadPool(size_t threadCount) : ThreadCount(threadCount)
{
    mAbort = false;
    mWorkingTasks = 0;
    mThreads.reserve(ThreadCount);
    while (mThreads.size() < ThreadCount) {
        auto taskProcess = [&] {
            while (doOneTask(mAbort)) {
            }
        };
        mThreads.emplace_back(taskProcess);
    };
}

ThreadPool::~ThreadPool()
{
    {
        std::scoped_lock<std::mutex> lock(mTaskMutex);
        mAbort = true;
    }

    mTaskCondition.notify_all();

    joinThreads();

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
    bool exitCondition = true;
    return doOneTask(exitCondition);
}

// TODO: Deprecate method and remove the working tasks atomic!
void ThreadPool::JoinTasks()
{
    bool exitCondition = true;
    while (doOneTask(exitCondition)) {
    }
    while (mWorkingTasks > 0) {
    }
}

bool ThreadPool::doOneTask(bool& exitThreadCondition)
{
    Task* task;
    {
        std::unique_lock<std::mutex> lock(mTaskMutex);
        mTaskCondition.wait(lock, [&]() { return exitThreadCondition || !mTaskQueue.empty(); });

        if (exitThreadCondition && mTaskQueue.empty()) {
            return false;
        }
        mWorkingTasks++;

        task = mTaskQueue.front();
        mTaskQueue.pop();
    }
    (*task)();
    mWorkingTasks--;
    return true;
}

void ThreadPool::joinThreads()
{
    assert(mAbort);
    for (auto& thread : mThreads) {
        thread.join();
    }
}
