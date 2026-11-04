#!/usr/bin/env python3
"""SmallTV bridge using the reviewed upstream usage readers. Never flashes firmware."""
import argparse
import importlib.util
import json
import logging
from logging.handlers import RotatingFileHandler
import os
from pathlib import Path
import signal
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import httpx

RUNTIME = Path(__file__).resolve().parent
STOP = threading.Event()
LOCK = threading.Lock()
STATE = {}
STALE_SECONDS = 300


def load_reader(provider):
    source = RUNTIME / 'readers' / (provider + '.py')
    spec = importlib.util.spec_from_file_location(provider + '_reader', source)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.LOG_PATH = RUNTIME / (provider + '-reader.log')
    return module


def snapshot():
    with LOCK:
        result = dict(STATE)
        result['payload'] = dict(STATE['payload'])
    age = time.time() - (result.get('last_success') or 0)
    result['fresh'] = age < STALE_SECONDS and result['payload'].get('ok', False)
    if not result['fresh']:
        result['payload'] = {'ok': False}
    return result


def update(**fields):
    with LOCK:
        STATE.update(fields)
    destination = RUNTIME / (STATE['provider'] + '-status.json')
    temporary = destination.with_suffix('.tmp')
    temporary.write_text(json.dumps(snapshot(), indent=2))
    os.chmod(temporary, 0o600)
    temporary.replace(destination)


def compatible(client, device):
    response = client.get(device + '/api/status')
    if response.status_code == 404:
        return False
    response.raise_for_status()
    return response.json().get('fw') == 'smalltv-ultra-mod'


def poll(provider, reader, server):
    if provider == 'codex':
        result = server.rate_limits()
        # Current app-server can return multiple buckets. Prefer the Codex one.
        buckets = result.get('rateLimitsByLimitId') or {}
        if 'codex' in buckets:
            result['rateLimits'] = buckets['codex']
        payload = reader.build_payload(result)
    else:
        token = reader.read_token()
        if not token:
            raise RuntimeError('Claude sign-in unavailable in macOS Keychain; sign in with Claude Code')
        payload, auth_failed = reader.poll_api(token)
        if auth_failed:
            raise RuntimeError('Claude sign-in expired or rejected; sign in again with Claude Code')
        if payload is None:
            raise RuntimeError('Claude usage request failed; see reader log')
    if not payload.get('ok'):
        raise RuntimeError('Provider returned no usage windows')
    return payload


def worker(args, reader):
    server = reader.CodexAppServer(reader.find_codex()) if args.provider == 'codex' else None
    next_poll = 0
    next_probe = 0
    supported = False
    path = '/api/usage' if args.provider == 'claude' else '/api/usage2'
    try:
        with httpx.Client(timeout=8, trust_env=False) as client:
            while not STOP.is_set():
                now = time.time()
                if now >= next_poll:
                    try:
                        payload = poll(args.provider, reader, server)
                        update(payload=payload, last_success=time.time(), error=None)
                        logging.info('usage refreshed: short=%s%% weekly=%s%%', payload.get('s'), payload.get('w'))
                    except Exception as exc:
                        update(error=str(exc))
                        logging.warning('usage refresh: %s', exc)
                    next_poll = now + args.interval
                if now >= next_probe:
                    try:
                        supported = compatible(client, args.device)
                        update(display='ready' if supported else 'waiting for compatible firmware')
                    except (httpx.HTTPError, ValueError) as exc:
                        supported = False
                        update(display='device unavailable: ' + str(exc))
                    next_probe = now + 60
                if supported:
                    try:
                        response = client.post(args.device + path, json=snapshot()['payload'])
                        response.raise_for_status()
                        update(display='push accepted', last_push=time.time())
                    except httpx.HTTPError as exc:
                        supported = False
                        next_probe = now + 60
                        update(display='push failed: ' + str(exc))
                STOP.wait(20)
    finally:
        if server:
            server.stop()


class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path != '/stop' or self.headers.get('X-SmallTV-Control') != STATE['provider']:
            self.send_error(403)
            return
        self.send_response(200)
        self.send_header('Content-Length', '0')
        self.end_headers()
        STOP.set()
        threading.Thread(target=self.server.shutdown, daemon=True).start()

    def do_GET(self):
        route = self.path.split('?')[0]
        if route == '/':
            body = (RUNTIME / 'dashboard.html').read_bytes()
            kind = 'text/html; charset=utf-8'
        elif route == '/codex-status' and STATE['provider'] == 'claude':
            try:
                response = httpx.get('http://127.0.0.1:8788/status', timeout=2, trust_env=False)
                body = response.content
            except httpx.HTTPError:
                body = json.dumps({'provider':'codex','fresh':False,'payload':{'ok':False},'error':'Codex service unavailable'}).encode()
            kind = 'application/json'
        elif route in ('/status', '/healthz', '/api/usage', '/api/usage2'):
            state = snapshot()
            body = json.dumps(state['payload'] if route.startswith('/api/') else state).encode()
            kind = 'application/json'
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header('Content-Type', kind)
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def log_message(self, *args):
        pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--provider', choices=['claude', 'codex'], required=True)
    parser.add_argument('--device', default='http://smalltv.local')
    parser.add_argument('--interval', type=int, default=60)
    args = parser.parse_args()
    port = 8787 if args.provider == 'claude' else 8788
    logging.basicConfig(level=logging.INFO, format='%(asctime)s %(message)s', handlers=[
        RotatingFileHandler(RUNTIME / (args.provider + '.log'), maxBytes=1_000_000, backupCount=2)])
    STATE.update(provider=args.provider, device=args.device, pid=os.getpid(), payload={'ok':False}, last_success=None, last_push=None,
                 display='checking device', error=None)
    reader = load_reader(args.provider)
    # The upstream Claude reader's log contains status/error text only; never tokens.
    if args.provider == 'claude':
        reader.log = lambda message: logging.info('%s', message)
    http = ThreadingHTTPServer(('127.0.0.1', port), Handler)
    def terminate(*_):
        STOP.set()
        threading.Thread(target=http.shutdown, daemon=True).start()
    signal.signal(signal.SIGTERM, terminate)
    signal.signal(signal.SIGINT, terminate)
    thread = threading.Thread(target=worker, args=(args, reader), daemon=True)
    thread.start()
    try:
        http.serve_forever(poll_interval=0.5)
    finally:
        STOP.set()
        thread.join(timeout=35)
        http.server_close()


if __name__ == '__main__':
    main()
