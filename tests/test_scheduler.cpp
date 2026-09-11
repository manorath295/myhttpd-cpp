#include "test_framework.hpp"
// tests/test_scheduler.cpp
#include "scheduler.hpp"
#include <atomic>
#include <chrono>
#include <thread>

using namespace myhttpd;

TEST(FcfsPolicy_FifoOrder) {
    FcfsPolicy p;
    std::deque<HttpRequest> q;
    HttpRequest a; a.file_size = 100; a.first_line = "A";
    HttpRequest b; b.file_size = 50;  b.first_line = "B";
    HttpRequest c; c.file_size = 200; c.first_line = "C";
    p.enqueue(q, a); p.enqueue(q, b); p.enqueue(q, c);
    EXPECT_EQ(p.next(q).first_line, std::string{"A"});
    EXPECT_EQ(p.next(q).first_line, std::string{"B"});
    EXPECT_EQ(p.next(q).first_line, std::string{"C"});
}

TEST(SjfPolicy_ServesSmallestFirst) {
    SjfPolicy p;
    std::deque<HttpRequest> q;
    HttpRequest a; a.file_size = 100; a.first_line = "A";
    HttpRequest b; b.file_size = 50;  b.first_line = "B";
    HttpRequest c; c.file_size = 200; c.first_line = "C";
    p.enqueue(q, a); p.enqueue(q, b); p.enqueue(q, c);
    EXPECT_EQ(p.next(q).first_line, std::string{"B"});
    EXPECT_EQ(p.next(q).first_line, std::string{"A"});
    EXPECT_EQ(p.next(q).first_line, std::string{"C"});
}

TEST(ReadyQueue_WaitBlocksUntilEnqueue) {
    ReadyQueue rq(std::make_unique<FcfsPolicy>());
    std::atomic<bool> done{false};
    std::thread t([&] {
        std::atomic<bool> stop{false};
        HttpRequest got = rq.wait_and_pop(stop);
        EXPECT_EQ(got.first_line, std::string{"hello"});
        done.store(true);
    });
    HttpRequest h; h.first_line = "hello";
    rq.enqueue(h);
    t.join();
    EXPECT_TRUE(done.load());
    EXPECT_EQ(std::string{rq.policy_name()}, std::string{"FCFS"});
}
