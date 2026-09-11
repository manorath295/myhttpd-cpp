#include "test_framework.hpp"
// tests/test_http_parser.cpp
#include "http_parser.hpp"
#include <cstdio>

using namespace myhttpd;

TEST(Helper_ParseMethod) {
    EXPECT_TRUE(parse_method("GET")  == HttpMethod::Get);
    EXPECT_TRUE(parse_method("HEAD") == HttpMethod::Head);
    EXPECT_TRUE(parse_method("POST") == HttpMethod::Unknown);
}

TEST(NormalizePath_Basics) {
    EXPECT_EQ(HttpParser::normalize_path("/"), std::string{});
    EXPECT_EQ(HttpParser::normalize_path("///foo/bar"), std::string{"foo/bar"});
    EXPECT_EQ(HttpParser::normalize_path("/foo/./bar/"), std::string{"foo/bar"});
    EXPECT_EQ(HttpParser::normalize_path("/foo?x=1"), std::string{"foo"});
    EXPECT_EQ(HttpParser::normalize_path("/foo%20bar"), std::string{"foo bar"});
}

TEST(NormalizePath_RejectsTraversal) {
    EXPECT_EQ(HttpParser::normalize_path("/../etc/passwd"), std::string{});
    EXPECT_EQ(HttpParser::normalize_path("/foo/../../bar"), std::string{});
}

TEST(Parse_BadRequest_NoLineEnd) {
    ClientIdentity cid{};
    auto r = HttpParser::parse("garbage", cid, "files");
    EXPECT_EQ(r.status_code, 400);
}

TEST(Parse_BadRequest_Malformed) {
    ClientIdentity cid{};
    auto r = HttpParser::parse("GARBAGE\r\n", cid, "files");
    EXPECT_EQ(r.status_code, 400);
}

TEST(Parse_MethodNotAllowed) {
    ClientIdentity cid{};
    auto r = HttpParser::parse("POST /x HTTP/1.1\r\n", cid, "files");
    EXPECT_EQ(r.status_code, 405);
    EXPECT_TRUE(r.method == HttpMethod::Unknown);
}

TEST(Parse_Ok) {
    ClientIdentity cid{};
    cid.ip = "127.0.0.1"; cid.port = 12345; cid.accept_fd = 7;
    auto r = HttpParser::parse("GET /sample.html HTTP/1.1\r\nHost: x\r\n\r\n", cid, "files");
    EXPECT_EQ(r.status_code, 0);          // 0 = ok, file will be checked at serve time
    EXPECT_TRUE(r.method == HttpMethod::Get);
    EXPECT_EQ(r.target, std::string{"/sample.html"});
    EXPECT_EQ(r.ip, std::string{"127.0.0.1"});
    EXPECT_EQ(r.port, 12345);
    EXPECT_EQ(r.accept_fd, 7);
}
