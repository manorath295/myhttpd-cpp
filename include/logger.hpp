// include/logger.hpp
// Thread-safe leveled logger with optional file output.
// Demonstrates the Singleton pattern (carefully) and RAII for the log file.

#pragma once

#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

namespace myhttpd {

enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3 };

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void set_level(LogLevel level) noexcept { level_ = level; }
    [[nodiscard]] LogLevel level() const noexcept { return level_; }

    void enable_file(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        file_.open(path, std::ios::app);
    }

    void disable_file() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) {
            file_.close();
        }
    }

    template <typename... Args>
    void log(LogLevel level, Args&&... args) {
        if (static_cast<int>(level) < static_cast<int>(level_)) return;
        std::ostringstream oss;
        oss << level_tag(level) << ' ';
        (oss << ... << args);
        oss << '\n';
        const std::string line = oss.str();

        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << line;
        if (file_.is_open()) {
            file_ << line;
            file_.flush();
        }
    }

    // Convenience helpers.
    template <typename... Args> void debug(Args&&... args) { log(LogLevel::Debug, std::forward<Args>(args)...); }
    template <typename... Args> void info (Args&&... args) { log(LogLevel::Info,  std::forward<Args>(args)...); }
    template <typename... Args> void warn (Args&&... args) { log(LogLevel::Warn,  std::forward<Args>(args)...); }
    template <typename... Args> void error(Args&&... args) { log(LogLevel::Error, std::forward<Args>(args)...); }

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static const char* level_tag(LogLevel l) {
        switch (l) {
            case LogLevel::Debug: return "[DEBUG]";
            case LogLevel::Info:  return "[INFO ]";
            case LogLevel::Warn:  return "[WARN ]";
            case LogLevel::Error: return "[ERROR]";
        }
        return "[?    ]";
    }

    LogLevel level_ = LogLevel::Info;
    std::mutex mutex_;
    std::ofstream file_;
};

} // namespace myhttpd
