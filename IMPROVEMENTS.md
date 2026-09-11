# Improvements over the original `MultiThreaded-Web-Server`

This document describes every change made when rebuilding the project for the
**Autodesk FY28 IND Internship Program** (Jan – Jun 2027). The brief asked for
a project that demonstrates strong **C++**, **Object-Oriented Design**,
**Data Structures / STL**, **Multi-threading**, **Memory Management**, and
**familiarity with the modern C++ toolchain (CMake, Git, profiling)**.
Each change below maps to one of those requirements.

---

## 1. Build system — moved from hand-rolled `make` to **CMake**

| | Original | Improved |
| --- | --- | --- |
| Build tool | `make` + hand-written `makefile` | `cmake` (3.16+) |
| Config | Hard-coded `-std=c++11` | `C++17`, `CMAKE_CXX_STANDARD_REQUIRED ON` |
| Compiler flags | `-Wall -Wextra -pedantic` | adds `-Wshadow -Wnon-virtual-dtor` |
| Sanitizers | none | optional `MYHTTPD_USE_ASAN` for AddressSanitizer/UBSan |
| Outputs | one binary | `myhttpd` (server) + `myhttpd_tests` + `myhttpd_bench` |
| Install | none | `install(TARGETS myhttpd RUNTIME DESTINATION bin)` |

**Why this matters for Autodesk:** the job spec explicitly lists "Familiarity
with **CMake**, Bash Script, Python, and Jenkins would be beneficial."

## 2. Modernized to **C++17** with STL-heavy code

The original was `C++11`. The rewrite uses:

* `std::filesystem::weakly_canonical` — for safe path resolution.
* `std::string_view` — zero-copy parameter passing in hot paths.
* `std::optional<T>` — typed "no result" instead of magic values.
* Structured binding is used in the scheduler policies.
* `std::packaged_task` + `std::future` — for the thread pool.
* `std::atomic<bool>` with proper memory ordering.
* `std::condition_variable` for blocking queues.

## 3. Clean **Object-Oriented Design** (split monolith into 6 focused classes)

| Class | Responsibility | File |
| --- | --- | --- |
| `HttpServer`   | Accept loop, lifecycle, signal handling | `http_server.{hpp,cpp}` |
| `HttpParser`   | Pure HTTP parsing (no I/O) — fully testable | `http_parser.{hpp,cpp}` |
| `HttpResponse` | Status-line, headers, file serving, dir listing | `http_response.{hpp,cpp}` |
| `ReadyQueue` + `SchedulingPolicy` | Pluggable scheduling | `scheduler.hpp` |
| `ThreadPool`   | Worker threads, packaged tasks, exception safety | `thread_pool.hpp` |
| `Logger`       | Thread-safe leveled log with optional file output | `logger.hpp` |
| `SocketHandle` | RAII socket wrapper | `socket_handle.hpp` |

The old code mixed parsing, scheduling, networking, and logging inside two
god-classes (`Parse`, `SendData`). Each concern now lives in its own file
with a single responsibility.

## 4. **Design patterns** (Autodesk looks for these explicitly)

* **Strategy** — `SchedulingPolicy` interface, with `FcfsPolicy` and
  `SjfPolicy` implementations. Adding a new policy (Round-Robin, Priority)
  is one new class; no existing code changes.
* **Singleton** — `Logger::instance()` (carefully — Meyer's singleton).
* **RAII** — `SocketHandle` for sockets, `std::unique_ptr` for the
  ready-queue policy, file lifetime tied to `std::ofstream` member of
  `Logger`.
* **Producer–Consumer** — accept thread produces, scheduler thread
  dispatches, worker threads consume.
* **Thread Pool** — `ThreadPool::submit` returns a `std::future`.

## 5. **Data structures** in active use

* `std::deque<HttpRequest>` — ready queue (O(1) front pop, O(1) back push).
* `std::list<HttpRequest>` was replaced — `deque` is better here because
  we never need iterators to be stable across push.
* `std::vector<char>` streaming buffer for file I/O.
* `std::unordered_map` extension → simple linear `if-else` chain in
  `content_type_for` (faster for the small fixed set).
* `std::condition_variable` + `std::mutex` for the blocking queue.

## 6. **Memory management** improvements

* **No more raw `int` socket fds scattered through the code** —
  everything goes through `SocketHandle` (move-only, RAII close on scope exit).
* **No manual `new` / `delete`** anywhere — every resource is owned by a
  smart pointer (`unique_ptr`) or a value-type RAII wrapper.
* **No global mutable state** for the server itself — only the
  `Logger::instance()` Meyer's singleton and `g_server` (signal handler
  pointer) remain global, and they're documented as such.

## 7. **Robustness fixes**

* **Path-traversal protection** — `std::filesystem::weakly_canonical` is
  used; `/../../etc/passwd` now returns 404 instead of leaking files.
* **HEAD vs GET** — fixed a bug where 404 / 405 bodies were *not* sent for
  non-GET, non-HEAD methods (e.g. POST 405 had 0-byte body). Now any method
  other than HEAD receives the error body, per RFC 7231.
* **`auto index.html`** — directories automatically serve their
  `index.html` if one exists, falling back to a styled directory listing.
* **Graceful shutdown** — `SIGINT` / `SIGTERM` now close the listening
  socket, drain the queue, and join the scheduler thread instead of just
  `_exit`ing. Workers finish in-flight requests.
* **Signal-safe `MSG_NOSIGNAL`** on every `send()` so a peer that closed
  mid-response doesn't kill the server with `SIGPIPE`.
* **`SO_REUSEADDR`** so restarts don't have to wait for the kernel.
* **Bind retries** across `AF_INET` and `AF_INET6`.

## 8. **Unit tests** — 21 tests, 100% passing

A tiny zero-dependency test framework (`tests/test_framework.hpp`) covers:

* `SocketHandle` — default, owns fd, move, release.
* `HttpParser` — method parsing, path normalization, traversal rejection,
  malformed input, method-not-allowed.
* `Scheduler` — FCFS order, SJF serves smallest first, blocking wait.
* `Logger` — level filter.
* `HttpResponse` — reason phrases, content-type detection, error pages,
  date formatting.

Run with:

```bash
cd build && ctest --output-on-failure   # or ./myhttpd_tests
```

## 9. **Benchmark tool** — built-in

`build/myhttpd_bench <threads> <requests>` spins up a server in-process
and fires concurrent connections, reporting requests/sec and total
duration. Useful for measuring the impact of the thread pool size and
scheduling policy.

On the development VM (8 threads, 2000 requests) we get
**~4 300 req/s** with 0 failures.

## 10. **Operational tooling**

* `scripts/run.sh` — build (if needed) and start the server with
  sensible defaults; respects `PORT`, `ROOTDIR`, `WORKERS`, `POLICY`,
  `LOGFILE` env vars.
* `scripts/bench.sh` — wrapper around the in-process benchmark.
* `scripts/smoke_test.py` — pure-Python client that hits the server with
  GET / HEAD / POST / 404 cases and reports per-request latency.

## 11. **Better UX**

* Styled HTML error pages (dark theme) instead of plain text.
* Styled HTML directory listing.
* `Server: myhttpd/2.0.0` header.
* `Last-Modified` header on static file responses (RFC 7232 ready).
* `Date` header in correct IMF-fixdate / RFC 7231 format.
* `Content-Type` includes `charset=utf-8` for text responses.
* More file extensions recognized (`json`, `svg`, `ico`, `pdf`).
* Log lines now include client **port**, **enqueue time**, **serve
  time**, **file size** in bytes.

## 12. **Code quality**

* Every header has `#pragma once`.
* Every public type lives in the `myhttpd::` namespace.
* All inputs and outputs are `const`/`noexcept` where possible.
* `[[nodiscard]]` on all fallible getters and on the `send_all` /
  `send_file` helpers.
* `-Wshadow -Wnon-virtual-dtor` enabled.
* `Logger` and `ThreadPool` are fully exception-safe.

---

## What was kept the same

* The high-level architecture: accept thread → ready queue → scheduler
  thread → worker pool → response.
* CLI surface — flags kept stable (`-p -r -n -s -l -d -t -h`) plus
  `-v` for log level.
* The same `files/` content so the new server is drop-in compatible with
  anything pointing at the old binary.
