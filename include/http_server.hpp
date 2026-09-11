// include/http_server.hpp
// Top-level HTTP server: owns socket, ready queue, thread pool, and
// runs the accept loop. Decoupled from parsing / responding for clarity.

#pragma once

#include "http_types.hpp"
#include "logger.hpp"
#include "scheduler.hpp"
#include "socket_handle.hpp"
#include "thread_pool.hpp"

#include <atomic>
#include <memory>
#include <netdb.h>
#include <string>
#include <thread>
#include <vector>

namespace myhttpd {

struct ServerConfig {
    std::string  port              = "8080";
    std::string  rootdir           = "files";
    std::string  scheduling_policy = "FCFS";   // FCFS | SJF
    std::size_t  worker_count      = 8;
    int          backlog           = 128;
    bool         daemon_mode       = false;   // single-request debug mode
    int          scheduler_warmup_sec = 0;     // 0 = start immediately
    std::string  log_file;                    // empty = no file logging
    LogLevel     log_level         = LogLevel::Info;
};

class HttpServer {
public:
    explicit HttpServer(ServerConfig cfg);
    ~HttpServer();

    // Bind, listen, and start workers. Returns false on bind error.
    bool start();

    // Stop accepting and wait for in-flight requests to drain.
    void stop() noexcept;

    // Run until stop() is called or a fatal error occurs.
    void run();

    // Convenience: for the -d single-request CLI mode.
    void handle_one_request();

    [[nodiscard]] const ServerConfig& config() const noexcept { return cfg_; }
    [[nodiscard]] bool is_running() const noexcept { return running_.load(); }

private:
    // Listen on a TCP port; returns -1 on error.
    int  bind_and_listen();
    // Read the HTTP request head (until \r\n\r\n).
    std::string read_request_head(int fd);
    // Format a per-request log line.
    static std::string format_log_line(const HttpRequest& r);
    // Process one accepted connection (used by both modes).
    void process_connection(int client_fd, std::string client_ip, std::uint16_t client_port);
    // Read until newline seen (used by accept loop for the daemon mode).
    static std::string read_line(int fd);

    ServerConfig                              cfg_;
    SocketHandle                              listener_;
    std::unique_ptr<ReadyQueue>               ready_;
    std::unique_ptr<ThreadPool>               workers_;
    std::thread                               scheduler_thread_;
    std::atomic<bool>                         running_{false};
    std::atomic<bool>                         shutdown_flag_{false};
    // Pre-bound port number, used to identify log entries.
    std::uint16_t                             bound_port_ = 0;
};

} // namespace myhttpd
