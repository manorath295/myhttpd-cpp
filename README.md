# myhttpd v2.0 — A Multithreaded HTTP Web Server in Modern C++17

> Built for the **Autodesk FY28 IND Internship Program** application.
> Demonstrates object-oriented design, modern C++, multithreading,
> memory-safe resource management, CMake, and unit testing.

`myhttpd` is a small but production-shaped multithreaded HTTP/1.1 web server
written from scratch in **C++17**. It listens on a TCP port, accepts HTTP
`GET` and `HEAD` requests, and serves files from a configured document root
through a pluggable scheduler (FCFS / SJF) and a worker thread pool.

| | |
| --- | --- |
| Language | C++17 |
| Build | CMake 3.16+ (no other dependencies) |
| Tests | 21 unit tests, 100% passing |
| Throughput | ~4 300 req/s on 8 threads (sample.html, localhost) |
| Lines of code | ~1 200 source + ~600 test |

---

## Features

* TCP socket accept loop with `SO_REUSEADDR` and IPv4/IPv6 dual-bind.
* Pluggable **scheduling policies** (`FCFS`, `SJF`) via the Strategy pattern.
* Fixed-size **thread pool** with `std::packaged_task` futures and graceful
  shutdown.
* **Path-traversal protection** using `std::filesystem::weakly_canonical`.
* **GET** and **HEAD** per RFC 7231; **405 Method Not Allowed** otherwise.
* Auto-served `index.html` for directories, with a styled HTML directory
  listing as fallback.
* Styled HTML error pages for 400 / 403 / 404 / 405 / 500.
* Thread-safe leveled logger (`DEBUG` / `INFO` / `WARN` / `ERROR`) with
  optional file output.
* Per-request access log line including client IP+port, enqueue time, serve
  time, status, file size.
* Graceful shutdown on `SIGINT` / `SIGTERM` — drains the queue and joins
  workers.

---

## Project structure

```
myhttpd/
├── CMakeLists.txt          # CMake build (C++17, warnings, ASan toggle)
├── include/                # Public headers (one class per file)
│   ├── http_server.hpp
│   ├── http_parser.hpp
│   ├── http_response.hpp
│   ├── http_types.hpp
│   ├── scheduler.hpp       # Strategy: FCFS / SJF
│   ├── thread_pool.hpp
│   ├── logger.hpp
│   ├── socket_handle.hpp   # RAII socket wrapper
│   └── version.hpp
├── src/                    # Implementations + main()
│   ├── main.cpp
│   ├── http_server.cpp
│   ├── http_parser.cpp
│   └── http_response.cpp
├── tests/                  # Unit tests
│   ├── test_framework.hpp  # Tiny TEST / EXPECT_EQ / EXPECT_TRUE
│   ├── test_main.cpp       # Runner
│   ├── test_socket_handle.cpp
│   ├── test_http_parser.cpp
│   ├── test_scheduler.cpp
│   ├── test_logger.cpp
│   └── test_http_response.cpp
├── benchmark/
│   └── bench_main.cpp      # In-process throughput benchmark
├── scripts/
│   ├── run.sh              # Build & start the server
│   ├── bench.sh            # Run the benchmark
│   └── smoke_test.py       # End-to-end Python client
├── files/                  # Sample document root
│   ├── index.html
│   ├── sample.html
│   ├── 2.html
│   ├── page5.html
│   ├── small.text
│   └── large.txt
├── IMPROVEMENTS.md         # What changed vs the original submission
├── .gitignore              # Excludes build outputs and local files from Git
└── README.md               # This file
```

---

## Requirements

* Linux / macOS / WSL
* `g++` or `clang++` with C++17 support (GCC 7+, Clang 5+)
* `cmake` 3.16 or newer
* `make` or `ninja`
* `pthread`
* `python3` (only for the smoke test)

---

## Build — step by step

### Ubuntu / WSL

From an Ubuntu WSL terminal, run these commands from the project directory:

```bash
cd /mnt/e/improved_project
sudo apt update
sudo apt install build-essential cmake python3
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The package installation is needed only once. The build creates the server,
unit-test executable, and benchmark executable inside `build/`.

```bash
# 1. Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo

# 2. Build (server, tests, benchmark)
cmake --build build -j

# 3. Run the unit tests
cd build && ctest --output-on-failure
# or: ./build/myhttpd_tests
```

Expected:

```
=== 21/21 tests passed ===
```

If you have GCC and want AddressSanitizer / UBSan:

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DMYHTTPD_USE_ASAN=ON
cmake --build build-asan -j
```

---

## Run — step by step

The easiest way is the helper script, but here is the manual path:

```bash
# From the project directory
cd /mnt/e/improved_project

# Start the server on port 8080 with 4 worker threads, FCFS scheduling
./build/myhttpd -p 8080 -r files -n 4 -s FCFS -v info

# In another terminal, hit it with curl
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/sample.html
curl -I http://127.0.0.1:8080/sample.html      # HEAD — headers only
curl -i http://127.0.0.1:8080/does-not-exist   # 404
curl -X POST -i http://127.0.0.1:8080/sample.html  # 405
```

Or with the helper script:

```bash
PORT=8080 ./scripts/run.sh
```

Stop the server with **Ctrl+C** (sends `SIGINT`, triggers graceful shutdown).

Open `http://127.0.0.1:8080/` in a web browser to view the served home page.
Keep the server terminal open while using the browser.



### CLI options

```
myhttpd v2.0.0 — multithreaded HTTP server

Usage: myhttpd [options]

  -p PORT      TCP port to listen on (default 8080)
  -r ROOTDIR   Document root (default ./files)
  -n N         Number of worker threads (default 8)
  -s POLICY    Scheduling policy: FCFS or SJF (default FCFS)
  -l FILE      Append logs to FILE
  -d           Debug mode: process a single request and exit
  -t SECONDS   Scheduler warmup in seconds (default 0)
  -v LEVEL     Log level: debug|info|warn|error (default info)
  -h           Show this help and exit
```

---

## Verify it works — end-to-end smoke test

Start the server in one shell:

```bash
./scripts/run.sh
```

In another shell:

```bash
python3 scripts/smoke_test.py --port 8080
```

Expected:

```
Smoke testing 127.0.0.1:8080
  ✓ GET   /                         200  (3.0 ms, 1264 bytes)
  ✓ GET   /sample.html              200  (1.0 ms, 343 bytes)
  ✓ GET   /does-not-exist           404  (0.6 ms, 564 bytes)
  ✓ HEAD  /sample.html              200  (0.5 ms, 0 bytes)
  ✓ POST  /sample.html              405  (0.4 ms, 582 bytes)

All tests passed
```

---

## Run the benchmark

```bash
./scripts/bench.sh                # default: 4 threads × 500 requests
THREADS=8 REQUESTS=2000 ./scripts/bench.sh
```

Sample output on a Linux VM:

```
Threads:    8
Requests:   2000
Succeeded:  2000
Failed:     0
Duration:   468 ms
Throughput: 4273.5 req/s
```

---

## Architecture

```
              ┌──────────────────────────┐
   client ──▶ │ HttpServer::run()        │
              │  - accept() in a loop    │
              │  - parse HTTP head       │
              │  - enqueue HttpRequest   │
              └────────────┬─────────────┘
                           │  push
                           ▼
              ┌──────────────────────────┐
              │ ReadyQueue<HttpRequest>  │  (FCFS or SJF policy)
              │  std::deque + cv + mutex │
              └────────────┬─────────────┘
                           │  pop
                           ▼
              ┌──────────────────────────┐
              │ scheduler thread         │
              │  - submit to thread pool │
              └────────────┬─────────────┘
                           │  submit(Future)
                           ▼
              ┌──────────────────────────┐
              │ ThreadPool (N workers)   │
              │  - HttpResponse::send()  │
              │  - close socket          │
              │  - log access line       │
              └──────────────────────────┘
```

* **Accept thread** never blocks on I/O: the request is read in a bounded
  loop until `\r\n\r\n` is seen, then immediately enqueued.
* **Scheduler thread** pops the next request (per the active policy) and
  submits it to the thread pool. The pool's `std::future` lets the
  scheduler return immediately.
* **Worker threads** build the response (header + file body), send it, and
  log the request line.
* **Shutdown** is cooperative: `SIGINT` sets an atomic flag, the accept
  loop breaks, the listener socket is closed, the scheduler drains the
  queue, and workers finish in-flight requests.

---

## Design patterns used

| Pattern | Where | Why |
| --- | --- | --- |
| **Strategy** | `SchedulingPolicy` (FCFS, SJF) | Pluggable scheduling; new policies = new class, no other changes. |
| **Singleton** (Meyer's) | `Logger::instance()` | Exactly one log sink per process. |
| **RAII** | `SocketHandle` | Automatic `close()` on scope exit, move-only. |
| **Producer–Consumer** | Accept → ReadyQueue → Scheduler → Workers | Classic bounded-buffer pattern with `std::condition_variable`. |
| **Thread Pool** | `ThreadPool` | Reuse worker threads; back-pressure is naturally limited by the queue. |
| **Command** | `ThreadPool::Task = std::function<void()>` | Workers execute arbitrary tasks submitted to the pool. |
| **Factory** (lightweight) | `parse_method` switch | Maps strings to enum values; trivially extensible. |

---

## Security & limitations

This is an educational server, not a production one. It is hardened
relative to the original, but you should still not expose it to the
open Internet without further work.

* **Path traversal**: blocked with `std::filesystem::weakly_canonical`.
* **`SIGPIPE`**: ignored; `send()` uses `MSG_NOSIGNAL`.
* **No TLS / HTTPS**.
* **No HTTP keep-alive** (each request uses a fresh `Connection: close`).
* **No request body parsing** (we never read past the header).
* **No virtual-host routing** (one document root per process).

---

## License

For the Autodesk FY28 IND Internship Program submission. Use freely for
educational purposes.
