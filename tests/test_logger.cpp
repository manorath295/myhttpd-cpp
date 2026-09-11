#include "test_framework.hpp"
// tests/test_logger.cpp
#include "logger.hpp"
#include "http_response.hpp"

using namespace myhttpd;

TEST(Logger_LevelFilter) {
    auto& lg = Logger::instance();
    lg.set_level(LogLevel::Warn);
    EXPECT_EQ(static_cast<int>(lg.level()), static_cast<int>(LogLevel::Warn));
    lg.set_level(LogLevel::Debug);
    EXPECT_EQ(static_cast<int>(lg.level()), static_cast<int>(LogLevel::Debug));
}

TEST(Logger_ReasonForCodes) {
    EXPECT_EQ(std::string{HttpResponse::reason_for(200)}, std::string{"OK"});
    EXPECT_EQ(std::string{HttpResponse::reason_for(404)}, std::string{"Not Found"});
    EXPECT_EQ(std::string{HttpResponse::reason_for(405)}, std::string{"Method Not Allowed"});
}

TEST(HttpResponse_ContentType) {
    EXPECT_EQ(HttpResponse::content_type_for("a.HTML"), std::string{"text/html; charset=utf-8"});
    EXPECT_EQ(HttpResponse::content_type_for("a.png"),  std::string{"image/png"});
    EXPECT_EQ(HttpResponse::content_type_for("a.json"), std::string{"application/json; charset=utf-8"});
    EXPECT_EQ(HttpResponse::content_type_for("a"),      std::string{"application/octet-stream"});
}
