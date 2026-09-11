// include/http_response.hpp
// Builds and sends HTTP responses. Encapsulates status code → reason,
// content-type detection, and date formatting.

#pragma once

#include "http_types.hpp"
#include "logger.hpp"
#include "socket_handle.hpp"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace myhttpd {

class HttpResponse {
public:
    // Send a fully built response. Closes the socket.
    static void send(const HttpRequest& req);

    // Build an HTML directory listing page.
    [[nodiscard]] static std::string directory_listing(const std::string& dirpath,
                                                        const std::string& url_path);

    // Build a styled HTML error page.
    [[nodiscard]] static std::string error_body(int code, std::string_view reason);

    // Map status code to a reason phrase.
    [[nodiscard]] static const char* reason_for(int code);

    // Look up a content-type from a file extension.
    [[nodiscard]] static std::string content_type_for(const std::string& filename);

    // RFC 7231 IMF-fixdate.
    [[nodiscard]] static std::string http_date(std::time_t t);

    // Send exactly n bytes, looping on partial writes.
    [[nodiscard]] static bool send_all(int fd, const char* data, std::size_t n);

    // Send the contents of a file to the socket.
    [[nodiscard]] static bool send_file(int fd, const std::string& path, long long size);
};

} // namespace myhttpd
