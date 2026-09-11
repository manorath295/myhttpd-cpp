// include/http_parser.hpp
// Pure (no I/O) HTTP request parser. Easy to unit test.

#pragma once

#include "http_types.hpp"
#include <string>
#include <string_view>

namespace myhttpd {

class HttpParser {
public:
    // Parse a raw request head into a partially-filled HttpRequest.
    // The socket fd and client metadata come from ClientIdentity.
    // Returns the request with status_code set to 200 on success, or 400 on a
    // malformed request line.
    static HttpRequest parse(std::string_view raw, const ClientIdentity& cid,
                             const std::string& rootdir);

    // Resolve the request target against the document root with traversal
    // protection. Returns std::nullopt if the path escapes the rootdir.
    [[nodiscard]] static std::optional<std::string>
    resolve_path(std::string_view target, const std::string& rootdir);

    // Strip query string, remove leading slashes, collapse "..", etc.
    [[nodiscard]] static std::string normalize_path(std::string_view path);

    // For tests: get the last 400 / 404 / 405 / 500 error page.
    [[nodiscard]] static std::string error_page(int code, std::string_view reason);
};

} // namespace myhttpd
