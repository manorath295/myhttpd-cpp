// src/http_response.cpp
#include "http_response.hpp"
#include "http_parser.hpp"
#include "version.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <vector>

namespace myhttpd {

const char* HttpResponse::reason_for(int code) {
    switch (code) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}

std::string HttpResponse::http_date(std::time_t t) {
    if (t == 0) t = std::time(nullptr);
    std::tm* gmt = std::gmtime(&t);
    char buf[64] = {};
    if (gmt && std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmt)) {
        return std::string(buf);
    }
    return "";
}

std::string HttpResponse::content_type_for(const std::string& filename) {
    const auto dot = filename.find_last_of('.');
    if (dot == std::string::npos) return "application/octet-stream";
    std::string ext = filename.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext == "html" || ext == "htm") return "text/html; charset=utf-8";
    if (ext == "txt" || ext == "text") return "text/plain; charset=utf-8";
    if (ext == "css")  return "text/css; charset=utf-8";
    if (ext == "js")   return "application/javascript; charset=utf-8";
    if (ext == "json") return "application/json; charset=utf-8";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif")  return "image/gif";
    if (ext == "png")  return "image/png";
    if (ext == "svg")  return "image/svg+xml";
    if (ext == "ico")  return "image/x-icon";
    if (ext == "pdf")  return "application/pdf";
    return "application/octet-stream";
}

bool HttpResponse::send_all(int fd, const char* data, std::size_t n) {
    std::size_t sent = 0;
    while (sent < n) {
        ssize_t w = ::send(fd, data + sent, n - sent, MSG_NOSIGNAL);
        if (w <= 0) {
            if (w < 0 && (errno == EINTR)) continue;
            return false;
        }
        sent += static_cast<std::size_t>(w);
    }
    return true;
}

bool HttpResponse::send_file(int fd, const std::string& path, long long size) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    constexpr std::size_t kBuf = 64 * 1024;
    std::vector<char> buf(kBuf);
    long long total = 0;
    while (in) {
        in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        std::streamsize got = in.gcount();
        if (got <= 0) break;
        if (!send_all(fd, buf.data(), static_cast<std::size_t>(got))) return false;
        total += got;
        if (size > 0 && total >= size) break;
    }
    return true;
}

std::string HttpResponse::error_body(int code, std::string_view reason) {
    return HttpParser::error_page(code, reason);
}

std::string HttpResponse::directory_listing(const std::string& dirpath,
                                            const std::string& url_path) {
    std::ostringstream os;
    os << "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"><title>"
       << "Index of " << url_path
       << "</title><style>"
       << "body{font-family:system-ui,sans-serif;background:#0f172a;color:#e2e8f0;margin:0;padding:32px}"
       << "h1{color:#f97316;border-bottom:1px solid #334155;padding-bottom:8px}"
       << "table{border-collapse:collapse;width:100%;max-width:720px}"
       << "td,th{padding:8px 12px;text-align:left;border-bottom:1px solid #1e293b}"
       << "th{background:#1e293b}"
       << "a{color:#38bdf8;text-decoration:none}"
       << "a:hover{text-decoration:underline}"
       << "</style></head><body>"
       << "<h1>Index of " << url_path << "</h1>"
       << "<table><thead><tr><th>Name</th><th>Type</th></tr></thead><tbody>";

    DIR* d = ::opendir(dirpath.c_str());
    if (d) {
        std::vector<std::string> entries;
        while (auto* e = ::readdir(d)) entries.emplace_back(e->d_name);
        ::closedir(d);
        std::sort(entries.begin(), entries.end(), [](const std::string& a, const std::string& b) {
            return std::lexicographical_compare(
                a.begin(), a.end(), b.begin(), b.end(),
                [](unsigned char x, unsigned char y) { return std::tolower(x) < std::tolower(y); });
        });
        for (const auto& name : entries) {
            if (name == ".") continue;
            std::string full = dirpath + "/" + name;
            struct stat st {};
            bool is_dir = (::stat(full.c_str(), &st) == 0) && S_ISDIR(st.st_mode);
            os << "<tr><td><a href=\"" << name << (is_dir ? "/" : "")
               << "\">" << name << (is_dir ? "/" : "") << "</a></td>"
               << "<td>" << (is_dir ? "directory" : "file") << "</td></tr>";
        }
    }
    os << "</tbody></table>"
       << "<p style=\"margin-top:24px;color:#64748b;font-size:13px\">"
       << "served by myhttpd/2.0</p></body></html>";
    return os.str();
}

void HttpResponse::send(const HttpRequest& req) {
    SocketHandle sock(req.accept_fd);
    if (!sock.valid()) return;

    HttpResponse dummy; (void)dummy; // placeholder if static helpers move

    auto build_header = [&](int code, const char* reason, std::size_t content_length,
                            const std::string& content_type,
                            const std::string& extra_headers = {}) {
        std::ostringstream h;
        h << "HTTP/1.1 " << code << " " << reason << "\r\n"
          << "Date: " << http_date(0) << "\r\n"
          << "Server: myhttpd/" MYHTTPD_VERSION "\r\n"
          << "Content-Type: " << content_type << "\r\n"
          << "Content-Length: " << content_length << "\r\n"
          << "Connection: close\r\n";
        if (!extra_headers.empty()) h << extra_headers;
        h << "\r\n";
        return h.str();
    };

    int  code    = req.status_code ? req.status_code : 200;
    auto reason  = reason_for(code);
    auto ctype   = content_type_for(req.resolved_path);

    if (code != 200) {
        std::string body = error_body(code, reason);
        std::string head = build_header(code, reason, body.size(), "text/html; charset=utf-8");
        send_all(sock.get(), head.data(), head.size());
        // Per RFC 7231: HEAD requests never carry a body; everything else does.
        if (req.method != HttpMethod::Head) {
            send_all(sock.get(), body.data(), body.size());
        }
        return;
    }

    struct stat st {};
    if (::stat(req.resolved_path.c_str(), &st) != 0) {
        code   = 404;
        reason = reason_for(code);
        std::string body = error_body(404, reason);
        std::string head = build_header(404, reason, body.size(), "text/html; charset=utf-8");
        send_all(sock.get(), head.data(), head.size());
        if (req.method != HttpMethod::Head) {
            send_all(sock.get(), body.data(), body.size());
        }
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        // Try to serve index.html in the directory first.
        std::string index_path = req.resolved_path + "/index.html";
        struct stat ist {};
        if (::stat(index_path.c_str(), &ist) == 0 && S_ISREG(ist.st_mode)) {
            long long isize = static_cast<long long>(ist.st_size);
            std::string ilm = http_date(ist.st_mtime);
            std::string extra = "Last-Modified: " + ilm + "\r\n";
            std::string head = build_header(200, "OK", static_cast<std::size_t>(isize),
                                            "text/html; charset=utf-8", extra);
            send_all(sock.get(), head.data(), head.size());
            if (req.method == HttpMethod::Get) {
                send_file(sock.get(), index_path, isize);
            }
            return;
        }
        std::string body = directory_listing(req.resolved_path, req.target);
        std::string head = build_header(200, "OK", body.size(), "text/html; charset=utf-8");
        send_all(sock.get(), head.data(), head.size());
        if (req.method != HttpMethod::Head) {
            send_all(sock.get(), body.data(), body.size());
        }
        return;
    }

    long long size = static_cast<long long>(st.st_size);
    std::string lm = http_date(st.st_mtime);
    std::string extra = "Last-Modified: " + lm + "\r\n";
    std::string head = build_header(200, "OK", static_cast<std::size_t>(size), ctype, extra);
    send_all(sock.get(), head.data(), head.size());
    if (req.method == HttpMethod::Get) {
        send_file(sock.get(), req.resolved_path, size);
    }
}

} // namespace myhttpd
