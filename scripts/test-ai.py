#!/usr/bin/env python3
"""Offline provider integration: real HTTP POST, MIDI compilation, errors and cancellation."""
import argparse
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import subprocess
import threading
import time

parser = argparse.ArgumentParser()
parser.add_argument('--build-dir', default='build')
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
binary = root / args.build_dir / 'fga_io_tests_artefacts' / 'Release' / ('fga_io_tests.exe' if __import__('os').name == 'nt' else 'fga_io_tests')
plan = {'title': 'Local fixture', 'summary': 'Six-minute techno', 'bpm': 138, 'key': 0, 'scale': 1, 'genre': 12, 'target_seconds': 360,
        'sections': [{'name': name, 'type': kind, 'bars': bars} for name, kind, bars in [('Intro', 'intro', 16), ('Drop', 'drop', 32), ('Outro', 'outro', 16)]],
        'patterns': [{'id': 'kick', 'track': 4, 'beats': 4, 'notes': [[0, 36, .5, 110], [1, 36, .5, 100], [2, 36, .5, 110], [3, 36, .5, 100]]}],
        'clips': [{'section': i, 'pattern': 'kick'} for i in range(3)]}

class Handler(BaseHTTPRequestHandler):
    mode = 'ok'
    seen = []
    def log_message(self, *args):
        pass
    def do_POST(self):
        body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        self.seen.append((self.path, body))
        if self.mode == 'stall':
            time.sleep(5)
        code = {'auth': 401, 'quota': 429, 'redirect': 307}.get(self.mode, 200) if self.path != '/capture' else 200
        data = b'invalid JSON' if self.mode == 'invalid' else json.dumps({'message': {'content': json.dumps(plan)}, 'done': True}).encode()
        try:
            self.send_response(code)
            self.send_header('Content-Type', 'application/json')
            if code == 307:
                self.send_header('Location', f'http://127.0.0.1:{self.server.server_port}/capture')
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass

server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
thread = threading.Thread(target=server.serve_forever, daemon=True)
thread.start()
endpoint = f'http://127.0.0.1:{server.server_port}'
try:
    for mode, expected in [('ok', ''), ('auth', 'API key'), ('quota', 'quota'), ('invalid', 'unreadable'), ('redirect', 'HTTP 307')]:
        Handler.mode = mode
        result = subprocess.run([str(binary), '--ai-http-test', endpoint], capture_output=True, text=True, timeout=15)
        assert (result.returncode == 0) == (mode == 'ok'), (mode, result.stderr)
        assert expected in result.stderr, (mode, result.stderr)
    assert len(Handler.seen) == 5
    for path, body in Handler.seen:
        assert path == '/api/chat' and body['stream'] is False and body['format'] == 'json'
        context = json.loads(body['messages'][1]['content'])
        assert context['task'] == 'techno 6 minutes'
        assert [t['id'] for t in context['enabled_tracks']] == [4, 17]
    Handler.mode = 'stall'
    start = time.monotonic()
    result = subprocess.run([str(binary), '--ai-http-cancel', endpoint], capture_output=True, text=True, timeout=8)
    assert result.returncode == 0 and time.monotonic() - start < 5, result.stderr
finally:
    server.shutdown()
    server.server_close()
print('PASS real local model HTTP: request context, six-minute MIDI, authentication/quota/errors, blocked redirects and cancellable stalled response')
