#include "test_framework.hpp"
// tests/test_http_response.cpp
#include "http_response.hpp"
#include "http_types.hpp"

using namespace myhttpd;

TEST(HttpResponse_ReasonFor) {
    EXPECT_EQ(std::string{HttpResponse::reason_for(200)}, std::string{"OK"});
    EXPECT_EQ(std::string{HttpResponse::reason_for(400)}, std::string{"Bad Request"});
    EXPECT_EQ(std::string{HttpResponse::reason_for(403)}, std::string{"Forbidden"});
    EXPECT_EQ(std::string{HttpResponse::reason_for(404)}, std::string{"Not Found"});
    EXPECT_EQ(std::string{HttpResponse::reason_for(500)}, std::string{"Internal Server Error"});
}

TEST(HttpResponse_ContentType_Common) {
    EXPECT_EQ(HttpResponse::content_type_for("index.htm"),  std::string{"text/html; charset=utf-8"});
    EXPECT_EQ(HttpResponse::content_type_for("file.txt"),   std::string{"text/plain; charset=utf-8"});
    EXPECT_EQ(HttpResponse::content_type_for("style.css"),  std::string{"text/css; charset=utf-8"});
    EXPECT_EQ(HttpResponse::content_type_for("app.js"),     std::string{"application/javascript; charset=utf-8"});
    EXPECT_EQ(HttpResponse::content_type_for("photo.jpg"),  std::string{"image/jpeg"});
    EXPECT_EQ(HttpResponse::content_type_for("photo.jpeg"), std::string{"image/jpeg"});
    EXPECT_EQ(HttpResponse::content_type_for("anim.gif"),   std::string{"image/gif"});
}

TEST(HttpResponse_ErrorBody_ContainsCode) {
    auto body = HttpResponse::error_body(404, "Not Found");
    EXPECT_TRUE(body.find("404") != std::string::npos);
    EXPECT_TRUE(body.find("Not Found") != std::string::npos);
    EXPECT_TRUE(body.find("<html") != std::string::npos);
}

TEST(HttpResponse_HttpDate_Format) {
    auto d = HttpResponse::http_date(0);
    // Format must end in GMT.
    EXPECT_TRUE(d.size() >= 29);
    EXPECT_EQ(d.substr(d.size() - 3), std::string{"GMT"});
}
