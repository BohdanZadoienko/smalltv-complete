#!/usr/bin/env python3
"""codexmeter — push OpenAI Codex usage to a smalltv-mod device.

Companion to clawdmeter-daemon: same transports, same payload contract, but the
numbers come from Codex and land on the device's /api/usage2 endpoint (the Codex
page) instead of /api/usage (the Claude page). Run both and the device carousel
shows Claude and Codex side by side.

Why it does not call the Codex HTTP API directly: chatgpt.com/backend-api sits
behind a bot filter that 403s any client whose TLS fingerprint isn't the real
CLI's — the exact headers, UA and OAuth token are not enough. The Codex CLI
already ships a local JSON-RPC surface (`codex app-server`) that answers
`account/rateLimits/read` from the same authenticated session, with no model
call and no token cost. That is what this polls.

  python codexmeter.py --push                    # mDNS-discover devices, push
  python codexmeter.py --push-to 192.168.1.50    # push to a fixed address
  python codexmeter.py --serve --port 8788       # serve for the device to pull
  python codexmeter.py --once                    # print one payload and exit

License: WTFPL
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import queue
import shutil
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import httpx

LOG_PATH = Path.home() / ".codexmeter.log"
DEFAULT_PORT = 8788
DEFAULT_INTERVAL = 60
DEFAULT_PUSH_INTERVAL = 20
PUSH_PATH = "/api/usage2"
MDNS_TYPE = "_clawdmeter._tcp.local."

log = logging.getLogger("codexmeter")


def setup_logging(verbose: bool) -> None:
    fmt = logging.Formatter("[%(asctime)s] %(message)s", "%Y-%m-%d %H:%M:%S")
    log.setLevel(logging.DEBUG if verbose else logging.INFO)
    for h in (logging.StreamHandler(sys.stdout), logging.FileHandler(LOG_PATH, encoding="utf-8")):
        h.setFormatter(fmt)
        log.addHandler(h)


# ---------------------------------------------------------------------------
# Codex app-server client
# ---------------------------------------------------------------------------
def find_codex() -> str | None:
    exe = shutil.which("codex")
    if exe:
        return exe
    # Default Windows install location, which is not always on PATH for a
    # service/autostart process.
    cand = Path(os.environ.get("LOCALAPPDATA", "")) / "Programs" / "OpenAI" / "Codex" / "bin" / "codex.exe"
    return str(cand) if cand.exists() else None


class CodexAppServer:
    """One long-lived `codex app-server` process, spoken to over stdio JSON-RPC.

    Kept alive between polls: the handshake costs a process spawn, the query
    itself is cheap. Any protocol error tears the process down so the next poll
    starts from a clean one.
    """

    def __init__(self, exe: str):
        self.exe = exe
        self.proc: subprocess.Popen | None = None
        self.rx: queue.Queue = queue.Queue()
        self._id = 0

    # -- process lifecycle --
    def _spawn(self) -> None:
        self.stop()
        log.debug("spawning %s app-server", self.exe)
        creationflags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
        self.proc = subprocess.Popen(
            [self.exe, "app-server"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
            text=True, encoding="utf-8", bufsize=1, creationflags=creationflags,
        )
        threading.Thread(target=self._pump, args=(self.proc.stdout,), daemon=True).start()
        self._id = 0
        self._request("initialize", {
            "clientInfo": {"name": "codexmeter", "title": "codexmeter", "version": "0.1.0"}
        })
        self._notify("initialized", {})

    def stop(self) -> None:
        if self.proc and self.proc.poll() is None:
            try:
                self.proc.terminate()
            except Exception:
                pass
        self.proc = None
        self.rx = queue.Queue()

    def _pump(self, stream) -> None:
        for line in stream:
            self.rx.put(line.strip())
        self.rx.put(None)

    # -- JSON-RPC --
    def _send(self, obj: dict) -> None:
        assert self.proc and self.proc.stdin
        self.proc.stdin.write(json.dumps(obj) + "\n")
        self.proc.stdin.flush()

    def _notify(self, method: str, params: dict) -> None:
        self._send({"jsonrpc": "2.0", "method": method, "params": params})

    def _request(self, method: str, params: dict, timeout: float = 30.0) -> dict:
        self._id += 1
        rid = self._id
        self._send({"jsonrpc": "2.0", "id": rid, "method": method, "params": params})
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                line = self.rx.get(timeout=1.0)
            except queue.Empty:
                continue
            if line is None:
                raise RuntimeError("app-server closed the connection")
            if not line:
                continue
            try:
                msg = json.loads(line)
            except json.JSONDecodeError:
                continue
            if msg.get("id") != rid:
                continue  # a notification (thread events, remote-control status, ...)
            if "error" in msg:
                raise RuntimeError(f"{method}: {msg['error']}")
            return msg.get("result") or {}
        raise TimeoutError(f"{method}: no reply in {timeout}s")

    def rate_limits(self) -> dict:
        if not self.proc or self.proc.poll() is not None:
            self._spawn()
        try:
            return self._request("account/rateLimits/read", {})
        except Exception:
            self.stop()          # poisoned pipe: next poll respawns
            raise


# ---------------------------------------------------------------------------
# Payload
# ---------------------------------------------------------------------------
def _window_minutes(now: float, window: dict | None) -> tuple[float, int] | None:
    """(usedPercent, minutes-until-reset) for one window, or None if absent."""
    if not isinstance(window, dict):
        return None
    pct = window.get("usedPercent")
    if pct is None:
        return None
    resets_at = window.get("resetsAt")
    mins = 0
    if isinstance(resets_at, (int, float)):
        mins = max(0, int((resets_at - now) / 60))
    return float(pct), mins


def build_payload(result: dict, now: float | None = None) -> dict:
    """Map a GetAccountRateLimitsResponse onto the device's usage contract.

    Codex names its windows primary/secondary, and which one is the 5-hour and
    which the weekly depends on the plan (some plans have only one). So the
    windows are sorted by their own declared duration rather than by name:
    anything up to 6 hours fills the "5h" slot, longer fills the "7d" slot.
    A slot the plan does not have is sent as -1 so the device drops that card.
    """
    now = time.time() if now is None else now
    limits = (result or {}).get("rateLimits") or {}

    session: tuple[float, int] | None = None
    weekly: tuple[float, int] | None = None
    for key in ("primary", "secondary"):
        w = limits.get(key)
        parsed = _window_minutes(now, w)
        if parsed is None:
            continue
        dur = (w or {}).get("windowDurationMins") or 0
        if dur and dur <= 360:
            session = parsed
        else:
            weekly = parsed

    if weekly is None and session is None:
        return {"s": -1, "sr": 0, "w": 0, "wr": 0, "st": "", "ok": False}
    # A plan with only a short window still needs the weekly slot filled, since
    # that is the one the device always draws.
    if weekly is None:
        weekly, session = session, None

    reached = limits.get("rateLimitReachedType")
    top = max(weekly[0], session[0] if session else 0.0)
    status = "rejected" if reached else ("allowed_warning" if top >= 90 else "allowed")

    return {
        "s": round(session[0]) if session else -1,
        "sr": session[1] if session else 0,
        "w": round(weekly[0]),
        "wr": weekly[1],
        "st": status,
        "ok": True,
        "pl": limits.get("planType") or "",
    }


NO_DATA = {"s": -1, "sr": 0, "w": 0, "wr": 0, "st": "", "ok": False}


# ---------------------------------------------------------------------------
# Transports
# ---------------------------------------------------------------------------
class Discovery:
    """mDNS browse for smalltv-mod devices (same service the Claude daemon uses)."""

    def __init__(self, enabled: bool):
        self.hosts: dict[str, str] = {}
        self.zc = None
        if not enabled:
            return
        try:
            from zeroconf import ServiceBrowser, ServiceListener, Zeroconf
        except ImportError:
            log.info("zeroconf not installed - discovery off (use --push-to)")
            return

        import socket

        outer = self

        class Listener(ServiceListener):
            def add_service(self, zc, type_, name):
                info = zc.get_service_info(type_, name, timeout=3000)
                if not info:
                    return
                addrs = [socket.inet_ntoa(a) for a in info.addresses if len(a) == 4]
                if not addrs:
                    return
                url = f"http://{addrs[0]}:{info.port or 80}{PUSH_PATH}"
                if outer.hosts.get(name) != url:
                    log.info("device found: %s -> %s", name, url)
                outer.hosts[name] = url

            update_service = add_service

            def remove_service(self, zc, type_, name):
                if outer.hosts.pop(name, None):
                    log.info("device gone: %s", name)

        self.zc = Zeroconf()
        ServiceBrowser(self.zc, MDNS_TYPE, Listener())
        log.info("mDNS discovery on - browsing for smalltv-mod devices")

    def targets(self) -> list[str]:
        return list(self.hosts.values())

    def close(self) -> None:
        if self.zc:
            self.zc.close()


def normalize_target(host: str) -> str:
    host = host.strip()
    if not host:
        return ""
    if not host.startswith(("http://", "https://")):
        host = "http://" + host
    if PUSH_PATH not in host:
        host = host.rstrip("/") + PUSH_PATH
    return host


class Latest:
    """The most recent payload, shared between the poller and --serve."""

    def __init__(self):
        self._lock = threading.Lock()
        self._data = dict(NO_DATA)

    def set(self, payload: dict) -> None:
        with self._lock:
            self._data = payload

    def get(self) -> dict:
        with self._lock:
            return dict(self._data)


def serve(latest: Latest, host: str, port: int) -> None:
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):  # noqa: N802
            body = json.dumps(latest.get() if self.path != "/healthz" else {"ok": True})
            payload = body.encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)

        def log_message(self, *a):
            pass

    srv = ThreadingHTTPServer((host, port), Handler)
    log.info("HTTP serve on %s:%d (device pulls %s)", host, port, PUSH_PATH)
    threading.Thread(target=srv.serve_forever, daemon=True).start()


# ---------------------------------------------------------------------------
def main() -> int:
    ap = argparse.ArgumentParser(description="Push Codex usage to a smalltv-mod device")
    ap.add_argument("--push", action="store_true", help="HTTP-push with mDNS auto-discovery")
    ap.add_argument("--push-to", action="append", default=[], metavar="DEVICE",
                    help="push to this device (IP/hostname). Repeatable and/or comma-separated")
    ap.add_argument("--no-discover", action="store_true", help="disable mDNS discovery")
    ap.add_argument("--push-interval", type=int, default=DEFAULT_PUSH_INTERVAL)
    ap.add_argument("--serve", action="store_true", help="run the HTTP server for pull mode")
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=DEFAULT_PORT)
    ap.add_argument("--interval", type=int, default=DEFAULT_INTERVAL,
                    help="seconds between Codex refreshes (default 60)")
    ap.add_argument("--once", action="store_true", help="print one payload and exit")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    setup_logging(args.verbose)

    exe = find_codex()
    if not exe:
        log.error("codex CLI not found - install it or put it on PATH")
        return 2

    static: list[str] = []
    for raw in args.push_to + [os.environ.get("CODEXMETER_PUSH_URL", "")]:
        for part in raw.split(","):
            t = normalize_target(part)
            if t and t not in static:
                static.append(t)

    push = args.push or bool(static)
    if not push and not args.serve and not args.once:
        args.serve = True   # same default as the Claude daemon

    server = CodexAppServer(exe)
    latest = Latest()

    if args.once:
        try:
            payload = build_payload(server.rate_limits())
        finally:
            server.stop()
        print(json.dumps(payload))
        return 0

    if args.serve:
        serve(latest, args.host, args.port)

    disc = Discovery(push and not args.no_discover)
    if push:
        log.info("HTTP push every %ds -> %s", args.push_interval,
                 ", ".join(static) if static else "(discovered devices)")

    client = httpx.Client(timeout=8)
    next_poll = 0.0
    next_push = 0.0
    fails = 0
    try:
        while True:
            now = time.time()

            if now >= next_poll:
                try:
                    payload = build_payload(server.rate_limits())
                    latest.set(payload)
                    fails = 0
                    log.info("5h=%s 7d=%d%% st=%s plan=%s",
                             f"{payload['s']}%" if payload["s"] >= 0 else "-",
                             payload["w"], payload["st"], payload["pl"])
                except Exception as e:
                    fails += 1
                    log.warning("codex poll failed (%d): %s", fails, e)
                    if fails >= 3:
                        latest.set(dict(NO_DATA))
                next_poll = now + args.interval

            if push and now >= next_push:
                targets = list(dict.fromkeys(static + disc.targets()))
                body = latest.get()
                for url in targets:
                    try:
                        r = client.post(url, json=body)
                        log.info("Pushing to %s %s", url, "OK" if r.status_code < 300 else r.status_code)
                    except Exception as e:
                        log.warning("push to %s failed: %s", url, e)
                if not targets:
                    log.debug("no push targets yet")
                next_push = now + args.push_interval

            time.sleep(1)
    except KeyboardInterrupt:
        return 0
    finally:
        client.close()
        disc.close()
        server.stop()


if __name__ == "__main__":
    sys.exit(main())
