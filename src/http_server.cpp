// src/http_server.cpp
#include "http_server.hpp"

#include "http_parser.hpp"
#include "http_response.hpp"
#include "version.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>
#include <netinet/in.h>
#include <signal.h>
#include <sstream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace myhttpd {

HttpServer::HttpServer(ServerConfig cfg) : cfg_(std::move(cfg)) {
    Logger::instance().set_level(cfg_.log_level);
    if (!cfg_.log_file.empty()) {
        Logger::instance().enable_file(cfg_.log_file);
    }
    if (cfg_.scheduling_policy == "SJF") {
        ready_ = std::make_unique<ReadyQueue>(std::make_unique<SjfPolicy>());
    } else {
        ready_ = std::make_unique<ReadyQueue>(std::make_unique<FcfsPolicy>());
    }
    workers_ = std::make_unique<ThreadPool>(cfg_.worker_count, "http-worker");
}

HttpServer::~HttpServer() { stop(); }

int HttpServer::bind_and_listen() {
    addrinfo hints {};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE | AI_NUMERICSERV;
    addrinfo* res = nullptr;
    int rc = ::getaddrinfo(nullptr, cfg_.port.c_str(), &hints, &res);
    if (rc != 0) {
        Logger::instance().error("getaddrinfo: ", gai_strerror(rc));
        return -1;
    }
    int fd = -1;
    for (auto* p = res; p; p = p->ai_next) {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        int yes = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        if (::bind(fd, p->ai_addr, p->ai_addrlen) == 0) {
            // Save the bound port.
            if (p->ai_family == AF_INET) {
                bound_port_ = ntohs(reinterpret_cast<sockaddr_in*>(p->ai_addr)->sin_port);
            } else {
                bound_port_ = ntohs(reinterpret_cast<sockaddr_in6*>(p->ai_addr)->sin6_port);
            }
            break;
        }
        ::close(fd);
        fd = -1;
    }
    ::freeaddrinfo(res);
    if (fd < 0) {
        Logger::instance().error("bind failed on port ", cfg_.port);
        return -1;
    }
    if (::listen(fd, cfg_.backlog) != 0) {
        Logger::instance().error("listen failed: ", std::strerror(errno));
        ::close(fd);
        return -1;
    }
    return fd;
}

std::string HttpServer::read_request_head(int fd) {
    std::string out;
    char buf[2048];
    while (out.size() < 16 * 1024) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        out.append(buf, static_cast<std::size_t>(n));
        if (out.find("\r\n\r\n") != std::string::npos) break;
        if (out.find("\n\n")     != std::string::npos) break;
    }
    return out;
}

std::string HttpServer::read_line(int fd) {
    std::string out;
    char c;
    while (out.size() < 1024) {
        ssize_t n = ::recv(fd, &c, 1, 0);
        if (n <= 0) break;
        if (c == '\n') break;
        if (c != '\r') out.push_back(c);
    }
    return out;
}

std::string HttpServer::format_log_line(const HttpRequest& r) {
    std::ostringstream os;
    os << r.ip << ':' << r.port
       << " [" << r.enqueue_time << "]"
       << " [" << r.serve_time << "]"
       << " \"" << r.first_line << "\""
       << ' ' << r.status_code
       << ' ' << r.file_size << 'B';
    return os.str();
}

void HttpServer::process_connection(int client_fd, std::string client_ip,
                                    std::uint16_t client_port) {
    std::string raw = read_request_head(client_fd);
    if (raw.empty()) {
        ::close(client_fd);
        return;
    }
    ClientIdentity cid{client_fd, client_ip, client_port, ""};
    {
        std::time_t t = std::time(nullptr);
        std::tm* gmt = std::gmtime(&t);
        char buf[64] = {};
        if (gmt) std::strftime(buf, sizeof(buf), "%x:%X", gmt);
        cid.request_time = buf;
    }
    HttpRequest req = HttpParser::parse(raw, cid, cfg_.rootdir);

    // Determine file size for SJF scheduling, file existence, etc.
    struct stat st {};
    if (req.status_code == 0) {
        if (::stat(req.resolved_path.c_str(), &st) == 0) {
            if (S_ISREG(st.st_mode)) {
                req.file_found = true;
                req.file_size  = static_cast<long long>(st.st_size);
            } else if (S_ISDIR(st.st_mode)) {
                req.file_found = true;
                req.file_size  = 0;
            }
        } else {
            // File resolved under rootdir but does not exist.
            req.status_code = 404;
        }
    }

    if (cfg_.daemon_mode) {
        // Send synchronously and exit.
        {
            std::time_t t = std::time(nullptr);
            std::tm* gmt = std::gmtime(&t);
            char buf[64] = {};
            if (gmt) std::strftime(buf, sizeof(buf), "%x:%X", gmt);
            req.serve_time = buf;
        }
        req.status_code = (req.status_code == 0) ? 200 : req.status_code;
        HttpResponse::send(req);
        Logger::instance().info(format_log_line(req));
        return;
    }

    ready_->enqueue(std::move(req));
}

void HttpServer::run() {
    if (cfg_.scheduler_warmup_sec > 0) {
        Logger::instance().info("Scheduler warmup for ", cfg_.scheduler_warmup_sec, "s");
        std::this_thread::sleep_for(std::chrono::seconds(cfg_.scheduler_warmup_sec));
    }

    int fd = bind_and_listen();
    if (fd < 0) {
        running_.store(false);
        return;
    }
    listener_.reset(fd);
    running_.store(true);
    Logger::instance().info("myhttpd v" MYHTTPD_VERSION " listening on port ",
                            bound_port_ ? std::to_string(bound_port_) : cfg_.port,
                            " (policy=", ready_->policy_name(),
                            ", workers=", cfg_.worker_count,
                            ", rootdir=", cfg_.rootdir, ")");

    // Scheduler thread: pops from the ready queue and dispatches to the pool.
    scheduler_thread_ = std::thread([this] {
        while (running_.load()) {
            HttpRequest req = ready_->wait_and_pop(shutdown_flag_);
            if (shutdown_flag_.load() && req.first_line.empty()) break;
            {
                std::time_t t = std::time(nullptr);
                std::tm* gmt = std::gmtime(&t);
                char buf[64] = {};
                if (gmt) std::strftime(buf, sizeof(buf), "%x:%X", gmt);
                req.serve_time = buf;
            }
            int fd_copy = req.accept_fd;
            try {
                workers_->submit([this, req = std::move(req), fd_copy]() mutable {
                    if (req.status_code == 0) req.status_code = 200;
                    HttpResponse::send(req);
                    Logger::instance().info(format_log_line(req));
                });
            } catch (const std::exception& e) {
                Logger::instance().error("submit failed: ", e.what());
                if (fd_copy >= 0) ::close(fd_copy);
            }
        }
    });

    while (running_.load()) {
        sockaddr_storage client_addr {};
        socklen_t       addr_len = sizeof(client_addr);
        int client_fd = ::accept(listener_.get(),
                                 reinterpret_cast<sockaddr*>(&client_addr),
                                 &addr_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            if (!running_.load()) break;
            Logger::instance().error("accept: ", std::strerror(errno));
            continue;
        }
        std::string ip;
        std::uint16_t port = 0;
        if (client_addr.ss_family == AF_INET) {
            char buf[INET_ADDRSTRLEN] = {};
            auto* a = reinterpret_cast<sockaddr_in*>(&client_addr);
            inet_ntop(AF_INET, &a->sin_addr, buf, sizeof(buf));
            ip = buf;
            port = ntohs(a->sin_port);
        } else {
            char buf[INET6_ADDRSTRLEN] = {};
            auto* a = reinterpret_cast<sockaddr_in6*>(&client_addr);
            inet_ntop(AF_INET6, &a->sin6_addr, buf, sizeof(buf));
            ip = buf;
            port = ntohs(a->sin6_port);
        }
        process_connection(client_fd, ip, port);
    }
    stop();
}

void HttpServer::handle_one_request() {
    cfg_.daemon_mode = true;
    int fd = bind_and_listen();
    if (fd < 0) return;
    listener_.reset(fd);
    running_.store(true);
    Logger::instance().info("myhttpd v" MYHTTPD_VERSION " debug mode on port ",
                            bound_port_ ? std::to_string(bound_port_) : cfg_.port);
    sockaddr_storage client_addr {};
    socklen_t       addr_len = sizeof(client_addr);
    int client_fd = ::accept(listener_.get(),
                             reinterpret_cast<sockaddr*>(&client_addr),
                             &addr_len);
    if (client_fd < 0) {
        Logger::instance().error("accept: ", std::strerror(errno));
        return;
    }
    std::string ip = "127.0.0.1";
    std::uint16_t port = 0;
    if (client_addr.ss_family == AF_INET) {
        char buf[INET_ADDRSTRLEN] = {};
        auto* a = reinterpret_cast<sockaddr_in*>(&client_addr);
        inet_ntop(AF_INET, &a->sin_addr, buf, sizeof(buf));
        ip = buf;
        port = ntohs(a->sin_port);
    }
    process_connection(client_fd, ip, port);
}

void HttpServer::stop() noexcept {
    if (!running_.exchange(false)) return;
    shutdown_flag_.store(true);
    if (listener_.valid()) {
        ::shutdown(listener_.get(), SHUT_RDWR);
        ::close(listener_.get());
        listener_.reset(-1);
    }
    if (ready_) ready_->notify_all();
    if (scheduler_thread_.joinable()) scheduler_thread_.join();
    if (workers_) workers_->shutdown();
    Logger::instance().info("myhttpd shutdown complete");
}

bool HttpServer::start() {
    int fd = bind_and_listen();
    if (fd < 0) return false;
    listener_.reset(fd);
    running_.store(true);
    return true;
}

} // namespace myhttpd
