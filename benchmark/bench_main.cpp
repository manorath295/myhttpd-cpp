// benchmark/bench_main.cpp
// In-process throughput benchmark: spins up the server on a random port
// and fires a flood of HTTP requests, reporting requests/sec and latency.
//
// Usage: myhttpd_bench [threads] [requests]

#include "http_server.hpp"
#include "logger.hpp"

#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <random>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

using myhttpd::HttpServer;
using myhttpd::ServerConfig;

namespace {
int pick_port() {
    // Bind to port 0 to let the OS assign a free port. We can't do that
    // through the server, so pick a random one in 30000-60000 and retry.
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(30000, 60000);
    return dist(rng);
}

bool send_request(int port, const std::string& path, std::string& out) {
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return false;
    sockaddr_in a {};
    a.sin_family = AF_INET;
    a.sin_port   = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    if (::connect(s, reinterpret_cast<sockaddr*>(&a), sizeof(a)) < 0) {
        ::close(s);
        return false;
    }
    std::string req = "GET " + path + " HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
    ::send(s, req.data(), req.size(), 0);
    char buf[4096];
    out.clear();
    while (true) {
        ssize_t n = ::recv(s, buf, sizeof(buf), 0);
        if (n <= 0) break;
        out.append(buf, static_cast<std::size_t>(n));
    }
    ::close(s);
    return !out.empty();
}
} // namespace

int main(int argc, char** argv) {
    int threads = 4;
    int requests = 1000;
    if (argc > 1) threads  = std::atoi(argv[1]);
    if (argc > 2) requests = std::atoi(argv[2]);

    int port = pick_port();
    ServerConfig cfg;
    cfg.port = std::to_string(port);
    cfg.rootdir = "files";
    cfg.worker_count = static_cast<std::size_t>(threads);
    cfg.scheduling_policy = "FCFS";
    cfg.log_level = myhttpd::LogLevel::Error;

    HttpServer server(cfg);
    std::thread t([&] { server.run(); });

    // Wait for server to come up.
    for (int i = 0; i < 100; ++i) {
        std::string r;
        if (send_request(port, "/sample.html", r)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::atomic<int> ok{0}, fail{0};
    auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> pool;
    for (int i = 0; i < threads; ++i) {
        pool.emplace_back([&] {
            for (int j = 0; j < requests / threads; ++j) {
                std::string r;
                if (send_request(port, "/sample.html", r) && r.find("200 OK") != std::string::npos) {
                    ok.fetch_add(1);
                } else {
                    fail.fetch_add(1);
                }
            }
        });
    }
    for (auto& th : pool) th.join();
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    double rps = (ms > 0) ? (ok.load() * 1000.0 / ms) : 0.0;
    std::cout << "Threads:    " << threads  << "\n"
              << "Requests:   " << requests << "\n"
              << "Succeeded:  " << ok.load() << "\n"
              << "Failed:     " << fail.load() << "\n"
              << "Duration:   " << ms << " ms\n"
              << "Throughput: " << rps << " req/s\n";

    server.stop();
    t.join();
    return 0;
}
