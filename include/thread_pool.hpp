// include/thread_pool.hpp
// A simple, fixed-size thread pool that supports graceful shutdown.
// Demonstrates RAII for worker threads and clean concurrency primitives.

#pragma once

#include "logger.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace myhttpd {

class ThreadPool {
public:
    using Task = std::function<void()>;

    explicit ThreadPool(std::size_t num_threads,
                        std::string name_prefix = "worker")
        : name_prefix_(std::move(name_prefix)) {
        if (num_threads == 0) num_threads = 1;
        workers_.reserve(num_threads);
        for (std::size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this, i] { worker_loop(i); });
        }
        Logger::instance().info("ThreadPool started with ", num_threads, " threads");
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Enqueue a task. Returns a future for the task's return value (or void).
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>> {
        using ReturnType = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        auto fut = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) {
                throw std::runtime_error("ThreadPool is shutting down");
            }
            tasks_.emplace([task] { (*task)(); });
        }
        cond_.notify_one();
        return fut;
    }

    void shutdown() noexcept {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) return;
            stopping_ = true;
        }
        cond_.notify_all();
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
        Logger::instance().info("ThreadPool stopped");
    }

    [[nodiscard]] std::size_t size() const noexcept { return workers_.size(); }
    [[nodiscard]] std::size_t pending() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.size();
    }

    ~ThreadPool() { shutdown(); }

private:
    void worker_loop(std::size_t idx) {
        (void)idx;
        for (;;) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cond_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
                if (stopping_ && tasks_.empty()) return;
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            try {
                task();
            } catch (const std::exception& e) {
                Logger::instance().error("Worker task threw: ", e.what());
            } catch (...) {
                Logger::instance().error("Worker task threw unknown exception");
            }
        }
    }

    std::vector<std::thread> workers_;
    std::queue<Task>         tasks_;
    mutable std::mutex       mutex_;
    std::condition_variable  cond_;
    std::atomic<bool>        stopping_{false};
    std::string              name_prefix_;
};

} // namespace myhttpd
