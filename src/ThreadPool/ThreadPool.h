#pragma once

#include <condition_variable>
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

private:
    inline bool threadDoTask(std::stop_token stopCondition);
    inline void joinThreads();

    std::mutex mTaskMutex;
    std::condition_variable mTaskCondition;

    std::vector<std::jthread> mThreads;
    std::queue<Task*> mTaskQueue;
};
