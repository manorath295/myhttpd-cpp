// src/http_parser.cpp
#include "http_parser.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace myhttpd {

std::string HttpParser::normalize_path(std::string_view path) {
    // 1. Strip query string.
    const auto qpos = path.find('?');
    if (qpos != std::string_view::npos) {
        path.remove_suffix(path.size() - qpos);
    }
    // 2. Drop any number of leading slashes.
    while (!path.empty() && path.front() == '/') path.remove_prefix(1);
    // 3. URL-decode minimal set (space, %20).
    std::string decoded;
    decoded.reserve(path.size());
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '%' && i + 2 < path.size()) {
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return 10 + c - 'a';
                if (c >= 'A' && c <= 'F') return 10 + c - 'A';
                return -1;
            };
            int hi = hex(path[i + 1]);
            int lo = hex(path[i + 2]);
            if (hi >= 0 && lo >= 0) {
                decoded.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        decoded.push_back(path[i]);
    }
    // 4. Split by '/' and resolve "." and "..".
    std::vector<std::string> parts;
    std::stringstream stream(decoded);
    std::string part;
    while (std::getline(stream, part, '/')) {
        if (part.empty() || part == ".") continue;
        if (part == "..") return ""; // explicit reject — caller treats as bad.
        parts.push_back(part);
    }
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) out += '/';
        out += parts[i];
    }
    return out;
}

std::optional<std::string>
HttpParser::resolve_path(std::string_view target, const std::string& rootdir) {
    std::string norm = normalize_path(target);
    // A request to "/" normalizes to empty — keep that and let the directory
    // handler serve index.html.

    fs::path root  = fs::weakly_canonical(fs::path(rootdir));
    fs::path final_path = norm.empty() ? root : fs::weakly_canonical(root / fs::path(norm));

    // Verify the resolved path stays under the document root.
    auto root_str  = root.generic_string();
    auto final_str = final_path.generic_string();
    if (final_str.size() < root_str.size()) return std::nullopt;
    if (final_str.compare(0, root_str.size(), root_str) != 0) {
        return std::nullopt;
    }
    return final_path.string();
}

HttpRequest HttpParser::parse(std::string_view raw, const ClientIdentity& cid,
                              const std::string& rootdir) {
    HttpRequest req;
    req.accept_fd = cid.accept_fd;
    req.ip        = cid.ip;
    req.port      = cid.port;
    req.enqueue_time = cid.request_time;

    const auto line_end = raw.find_first_of("\r\n");
    if (line_end == std::string_view::npos) {
        req.status_code = 400;
        req.first_line  = std::string(raw);
        return req;
    }
    req.first_line = std::string(raw.substr(0, line_end));

    std::istringstream iss(req.first_line);
    std::string method_str, target, version;
    if (!(iss >> method_str >> target >> version)) {
        req.status_code = 400;
        return req;
    }
    req.method_raw = method_str;
    req.method     = parse_method(method_str);
    req.target     = target;
    req.version    = version;

    if (req.method == HttpMethod::Unknown) {
        req.status_code = 405;
        return req;
    }

    auto resolved = resolve_path(target, rootdir);
    if (!resolved) {
        req.status_code = 404;
        return req;
    }
    req.resolved_path = *resolved;
    return req;
}

std::string HttpParser::error_page(int code, std::string_view reason) {
    std::ostringstream os;
    os << "<!DOCTYPE html>\n"
       << "<html><head><meta charset=\"utf-8\"><title>"
       << code << " " << reason
       << "</title><style>"
       << "body{font-family:sans-serif;background:#0f172a;color:#e2e8f0;"
       << "display:flex;align-items:center;justify-content:center;height:100vh;margin:0}"
       << ".card{background:#1e293b;padding:40px 60px;border-radius:12px;"
       << "box-shadow:0 10px 25px rgba(0,0,0,.4);text-align:center}"
       << "h1{font-size:64px;margin:0 0 10px;color:#f97316}"
       << "p{font-size:18px;color:#94a3b8}"
       << "</style></head><body><div class=\"card\">"
       << "<h1>" << code << "</h1>"
       << "<p>" << reason << "</p>"
       << "<p style=\"margin-top:20px;font-size:13px\">myhttpd/2.0</p>"
       << "</div></body></html>";
    return os.str();
}

} // namespace myhttpd
