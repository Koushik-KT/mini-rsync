#include "include/threadpool.h"
#include <iostream>

ThreadPool::ThreadPool(size_t n) {
    for (size_t i = 0; i < n; ++i) {
        workers.emplace_back([this]() { workerLoop(); });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lk(mu);
        stop = true;
    }
    cv.notify_all();
    for (auto &t : workers) if (t.joinable()) t.join();
}

void ThreadPool::enqueue(std::function<void()> job) {
    {
        std::unique_lock<std::mutex> lk(mu);
        tasks.push(std::move(job));
    }
    cv.notify_one();
}

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lk(mu);
            cv.wait(lk, [this]{ return stop || !tasks.empty(); });
            if (stop && tasks.empty()) return;
            job = std::move(tasks.front());
            tasks.pop();
        }
        try {
            job();
        } catch (const std::exception &e) {
            std::cerr << "Worker job error: " << e.what() << std::endl;
        }
    }
}
