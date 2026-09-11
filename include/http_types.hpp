// include/http_types.hpp
// Core data structures used across the server.
// Uses modern C++ (std::optional, std::string_view, enum class).

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace myhttpd {

enum class HttpMethod { Get, Head, Unknown };

// Convert a textual method to enum.
[[nodiscard]] inline HttpMethod parse_method(std::string_view s) {
    if (s == "GET")  return HttpMethod::Get;
    if (s == "HEAD") return HttpMethod::Head;
    return HttpMethod::Unknown;
}

[[nodiscard]] inline const char* method_name(HttpMethod m) {
    switch (m) {
        case HttpMethod::Get:  return "GET";
        case HttpMethod::Head: return "HEAD";
        case HttpMethod::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

struct ClientIdentity {
    int         accept_fd = -1;
    std::string ip;
    std::uint16_t port = 0;
    std::string request_time;
};

// Plain DTO describing a parsed HTTP request. Owned by the scheduler / worker.
struct HttpRequest {
    HttpMethod                 method   = HttpMethod::Unknown;
    std::string                method_raw;
    std::string                target;
    std::string                version;
    std::string                first_line;
    std::string                resolved_path;     // Path joined with rootdir, normalized.
    std::string                ip;
    std::uint16_t              port    = 0;
    int                        accept_fd = -1;
    std::string                enqueue_time;
    std::string                serve_time;
    int                        status_code = 0;    // Filled by sender.
    long long                  file_size   = 0;    // Bytes; 0 if N/A.
    bool                       file_found  = false;
};

} // namespace myhttpd
