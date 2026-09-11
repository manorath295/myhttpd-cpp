#include "test_framework.hpp"
// tests/test_socket_handle.cpp
#include "socket_handle.hpp"
#include <unistd.h>

using myhttpd::SocketHandle;

TEST(SocketHandle_DefaultIsInvalid) {
    SocketHandle s;
    EXPECT_TRUE(!s.valid());
    EXPECT_EQ(s.get(), -1);
}

TEST(SocketHandle_OwnsFileDescriptor) {
    int fds[2];
    EXPECT_TRUE(::pipe(fds) == 0);
    SocketHandle r(fds[0]), w(fds[1]);
    EXPECT_TRUE(r.valid());
    EXPECT_TRUE(w.valid());
    // Write something and read it back through the RAII handles.
    const char* msg = "hi";
    EXPECT_EQ(::write(w.get(), msg, 2), 2);
    char buf[2] = {};
    EXPECT_EQ(::read(r.get(), buf, 2), 2);
    EXPECT_EQ(buf[0], 'h');
    EXPECT_EQ(buf[1], 'i');
    // Going out of scope closes both fds.
}

TEST(SocketHandle_ReleaseYieldsFd) {
    int fds[2];
    EXPECT_TRUE(::pipe(fds) == 0);
    SocketHandle h(fds[0]);
    int raw = h.release();
    EXPECT_EQ(raw, fds[0]);
    EXPECT_TRUE(!h.valid());
    ::close(raw);
    ::close(fds[1]);
}

TEST(SocketHandle_MoveTransfersOwnership) {
    int fds[2];
    EXPECT_TRUE(::pipe(fds) == 0);
    SocketHandle a(fds[0]);
    SocketHandle b(std::move(a));
    EXPECT_TRUE(!a.valid());
    EXPECT_TRUE(b.valid());
    EXPECT_EQ(b.get(), fds[0]);
    ::close(fds[1]);
}
