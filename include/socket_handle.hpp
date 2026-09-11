// include/socket_handle.hpp
// RAII wrapper for POSIX socket file descriptors.
// Demonstrates the Resource Acquisition Is Initialization (RAII) idiom
// — one of the C++ Design Patterns Autodesk looks for.

#pragma once

#include <unistd.h>

namespace myhttpd {

class SocketHandle {
public:
    SocketHandle() noexcept = default;
    explicit SocketHandle(int fd) noexcept : fd_(fd) {}

    // Non-copyable: a socket cannot be duplicated.
    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;

    // Movable: ownership can be transferred.
    SocketHandle(SocketHandle&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }
    SocketHandle& operator=(SocketHandle&& other) noexcept {
        if (this != &other) {
            close();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    ~SocketHandle() { close(); }

    [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }
    [[nodiscard]] int  get() const noexcept { return fd_; }

    // Release ownership of the underlying fd without closing it.
    [[nodiscard]] int release() noexcept {
        const int released = fd_;
        fd_ = -1;
        return released;
    }

    void reset(int new_fd = -1) noexcept {
        if (fd_ != new_fd) {
            close();
            fd_ = new_fd;
        }
    }

    void close() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

private:
    int fd_ = -1;
};

} // namespace myhttpd
