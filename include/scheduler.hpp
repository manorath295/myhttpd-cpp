// include/scheduler.hpp
// Strategy pattern: pluggable scheduling policies (FCFS, SJF, Priority).
// Each policy is a small class implementing select_next(); the
// ReadyQueue just delegates to the active strategy.

#pragma once

#include "http_types.hpp"
#include "logger.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>

namespace myhttpd {

// Abstract strategy.
class SchedulingPolicy {
public:
    virtual ~SchedulingPolicy() = default;
    virtual void enqueue(std::deque<HttpRequest>& q, HttpRequest req) = 0;
    virtual HttpRequest next(std::deque<HttpRequest>& q) = 0;
    [[nodiscard]] virtual const char* name() const = 0;
};

class FcfsPolicy final : public SchedulingPolicy {
public:
    void enqueue(std::deque<HttpRequest>& q, HttpRequest req) override {
        q.push_back(std::move(req));
    }
    HttpRequest next(std::deque<HttpRequest>& q) override {
        auto r = std::move(q.front());
        q.pop_front();
        return r;
    }
    [[nodiscard]] const char* name() const override { return "FCFS"; }
};

class SjfPolicy final : public SchedulingPolicy {
public:
    void enqueue(std::deque<HttpRequest>& q, HttpRequest req) override {
        q.push_back(std::move(req));
    }
    HttpRequest next(std::deque<HttpRequest>& q) override {
        // Linear search for the smallest known file size.
        std::size_t best = 0;
        for (std::size_t i = 1; i < q.size(); ++i) {
            if (q[i].file_size < q[best].file_size) best = i;
        }
        auto r = std::move(q[best]);
        q.erase(q.begin() + static_cast<std::ptrdiff_t>(best));
        return r;
    }
    [[nodiscard]] const char* name() const override { return "SJF"; }
};

// Thread-safe request queue. Workers block on wait_for_request().
class ReadyQueue {
public:
    explicit ReadyQueue(std::unique_ptr<SchedulingPolicy> policy)
        : policy_(std::move(policy)) {}

    void set_policy(std::unique_ptr<SchedulingPolicy> p) {
        std::lock_guard<std::mutex> lock(mutex_);
        policy_ = std::move(p);
    }

    void enqueue(HttpRequest req) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            policy_->enqueue(queue_, std::move(req));
        }
        cond_.notify_one();
    }

    // Block until a request is available or shutdown is requested.
    HttpRequest wait_and_pop(std::atomic<bool>& shutdown_flag) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [&] { return shutdown_flag.load() || !queue_.empty(); });
        if (queue_.empty()) {
            return HttpRequest{}; // shutdown sentinel
        }
        return policy_->next(queue_);
    }

    void notify_all() { cond_.notify_all(); }

    [[nodiscard]] std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    [[nodiscard]] const char* policy_name() const { return policy_->name(); }

private:
    mutable std::mutex      mutex_;
    std::condition_variable cond_;
    std::deque<HttpRequest> queue_;
    std::unique_ptr<SchedulingPolicy> policy_;
};

} // namespace myhttpd
