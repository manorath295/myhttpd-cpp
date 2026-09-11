#!/usr/bin/env python3
"""scripts/smoke_test.py — simple Python client that exercises myhttpd."""

import argparse
import http.client
import json
import sys
import time


def fetch(host: str, port: int, path: str, method: str = "GET") -> tuple[int, str]:
    conn = http.client.HTTPConnection(host, port, timeout=5)
    try:
        conn.request(method, path)
        resp = conn.getresponse()
        body = resp.read()
        return resp.status, body.decode("utf-8", errors="replace")
    finally:
        conn.close()


def main() -> int:
    p = argparse.ArgumentParser(description="Smoke test the myhttpd server")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=8080)
    p.add_argument("--path", default="/")
    args = p.parse_args()

    tests = [
        ("GET",  "/",                 200),
        ("GET",  "/sample.html",      200),
        ("GET",  "/does-not-exist",   404),
        ("HEAD", "/sample.html",      200),
        ("POST", "/sample.html",      405),
    ]

    print(f"Smoke testing {args.host}:{args.port}")
    failures = 0
    for method, path, expected in tests:
        t0 = time.time()
        try:
            code, body = fetch(args.host, args.port, path, method)
        except Exception as exc:  # pragma: no cover
            print(f"  ✗ {method:5s} {path:25s} ERROR  ({exc})")
            failures += 1
            continue
        dt = (time.time() - t0) * 1000
        ok = code == expected
        marker = "✓" if ok else "✗"
        print(f"  {marker} {method:5s} {path:25s} {code}  ({dt:.1f} ms, {len(body)} bytes)")
        if not ok:
            failures += 1

    print()
    if failures:
        print(f"{failures} test(s) failed")
        return 1
    print("All tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
