import importlib.util
import tempfile
import threading
import time
import unittest
from pathlib import Path
from unittest.mock import patch

import httpx

spec = importlib.util.spec_from_file_location('bridge', Path(__file__).parent.parent / 'macos/usage_service.py')
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)
PAYLOAD = {'s': 6, 'sr': 120, 'w': 92, 'wr': 80, 'st': 'allowed_warning', 'ok': True, 'pl': 'plus'}


class BridgeTest(unittest.TestCase):
    def run_cycle(self, status, identity=None):
        seen = []
        def transport(request):
            seen.append(request)
            if request.method == 'GET':
                return httpx.Response(status, json={'fw': identity})
            return httpx.Response(200, json={'ok': True})
        client = httpx.Client(transport=httpx.MockTransport(transport))
        class Reader:
            class CodexAppServer:
                def __init__(self, _): pass
                def stop(self): pass
            def find_codex(self): return '/codex'
        class Args:
            provider = 'codex'
            device = 'http://192.0.2.1'
            interval = 60
        with tempfile.TemporaryDirectory() as directory:
            bridge.STATE.clear()
            bridge.STATE.update(provider='codex', payload={'ok': False}, last_success=None, last_push=None)
            stop = threading.Event()
            with patch.object(bridge, 'RUNTIME', Path(directory)), patch.object(bridge, 'STOP', stop), \
                    patch.object(bridge, 'poll', return_value=PAYLOAD), \
                    patch.object(bridge.httpx, 'Client', return_value=client), \
                    patch.object(stop, 'wait', side_effect=lambda *_: stop.set()):
                bridge.worker(Args(), Reader())
        return seen

    def test_stock_firmware_is_never_posted_to(self):
        self.assertEqual([r.method for r in self.run_cycle(404)], ['GET'])

    def test_other_firmware_is_never_posted_to(self):
        self.assertEqual([r.method for r in self.run_cycle(200, 'smalltv-mod')], ['GET'])

    def test_dashboard_firmware_receives_only_usage_payload(self):
        requests = self.run_cycle(200, 'smalltv-ultra-mod')
        self.assertEqual([(r.method, r.url.path) for r in requests], [('GET', '/api/status'), ('POST', '/api/usage2')])
        import json
        self.assertEqual(json.loads(requests[1].content), PAYLOAD)
        self.assertNotIn('authorization', requests[1].headers)

    def test_expired_readings_are_marked_unavailable(self):
        bridge.STATE.update(payload=PAYLOAD, last_success=time.time()-301)
        self.assertEqual(bridge.snapshot()['payload'], {'ok': False})
        self.assertFalse(bridge.snapshot()['fresh'])


if __name__ == '__main__':
    unittest.main()
