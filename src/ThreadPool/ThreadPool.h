#pragma once

#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

typedef std::function<void()> Task;

class ThreadPool final
{
public:
    ThreadPool(size_t threadCount);
    ~ThreadPool();

    const size_t ThreadCount;

    void PushTask(Task& task);
    // TODO: The following is open to a few problems, among others, if tasks come from vectors references are not stable
    void PushTasks(std::vector<Task>::iterator first, std::vector<Task>::iterator last);
    bool TryHelpOneTask();
    void JoinTasks(); // TODO: Remove? Replace with one that takes a set or id for tasks or a predicate?

private:
    __forceinline bool doOneTask(bool& stopCondition);
    __forceinline void joinThreads();

    std::mutex mTaskMutex;
    std::condition_variable mTaskCondition;

    bool mAbort;
    std::vector<std::thread> mThreads;
    std::queue<Task*> mTaskQueue;
    std::atomic<uint32_t> mWorkingTasks; // This is only needed for purposes of JoinTasks.
};
