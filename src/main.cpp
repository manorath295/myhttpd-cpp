// src/main.cpp
// CLI entry point. Demonstrates getopt, signal handling, configuration parsing.

#include "http_server.hpp"
#include "logger.hpp"
#include "version.hpp"

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

using myhttpd::HttpServer;
using myhttpd::Logger;
using myhttpd::LogLevel;
using myhttpd::ServerConfig;

namespace {
HttpServer* g_server = nullptr;

void on_signal(int sig) {
    if (g_server) g_server->stop();
    (void)sig;
}

int to_positive_int(const char* s, int fallback) {
    if (!s || !*s) return fallback;
    char* end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (end == s || *end != '\0' || v <= 0) return fallback;
    return static_cast<int>(v);
}

void print_help() {
    std::cout <<
R"(myhttpd v)" MYHTTPD_VERSION R"( — multithreaded HTTP server

Usage: myhttpd [options]

  -p PORT      TCP port to listen on (default 8080)
  -r ROOTDIR   Document root (default ./files)
  -n N         Number of worker threads (default 8)
  -s POLICY    Scheduling policy: FCFS or SJF (default FCFS)
  -l FILE      Append logs to FILE
  -d           Debug mode: process a single request and exit
  -t SECONDS   Scheduler warmup in seconds (default 0)
  -v LEVEL     Log level: debug|info|warn|error (default info)
  -h           Show this help and exit
)";
}

LogLevel parse_level(const char* s) {
    std::string v = s;
    for (auto& c : v) c = static_cast<char>(std::tolower(c));
    if (v == "debug") return LogLevel::Debug;
    if (v == "info")  return LogLevel::Info;
    if (v == "warn" || v == "warning") return LogLevel::Warn;
    if (v == "error") return LogLevel::Error;
    return LogLevel::Info;
}
} // namespace

int main(int argc, char** argv) {
    ServerConfig cfg;
    int opt;
    while ((opt = ::getopt(argc, argv, "p:r:n:s:l:t:v:dh")) != -1) {
        switch (opt) {
            case 'p': cfg.port = optarg; break;
            case 'r': cfg.rootdir = optarg; break;
            case 'n': cfg.worker_count = static_cast<std::size_t>(to_positive_int(optarg, 8)); break;
            case 's': cfg.scheduling_policy = optarg; break;
            case 'l': cfg.log_file = optarg; break;
            case 'd': cfg.daemon_mode = true; break;
            case 't': cfg.scheduler_warmup_sec = to_positive_int(optarg, 0); break;
            case 'v': cfg.log_level = parse_level(optarg); break;
            case 'h': print_help(); return 0;
            default:  print_help(); return 1;
        }
    }
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);
    std::signal(SIGPIPE, SIG_IGN);

    // Convert policy to upper case for case-insensitive comparison.
    for (auto& c : cfg.scheduling_policy) c = static_cast<char>(std::toupper(c));
    if (cfg.scheduling_policy != "FCFS" && cfg.scheduling_policy != "SJF") {
        std::cerr << "Unknown scheduling policy: " << cfg.scheduling_policy
                  << " (use FCFS or SJF)\n";
        return 1;
    }

    HttpServer server(cfg);
    g_server = &server;
    Logger::instance().info("myhttpd v" MYHTTPD_VERSION " starting (pid=", ::getpid(), ")");
    if (cfg.daemon_mode) {
        server.handle_one_request();
    } else {
        server.run();
    }
    return 0;
}
