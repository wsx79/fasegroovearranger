#!/usr/bin/env python3
"""Offline provider integration: real HTTP POST, MIDI compilation, errors and cancellation."""
import argparse
import json
import os
import socket
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import subprocess
import threading
import time

parser = argparse.ArgumentParser()
parser.add_argument('--build-dir', default='build')
parser.add_argument('--ui-output', help='Also run the native UI regression under an existing display')
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
    def do_GET(self):
        assert self.path == '/api/tags', self.path
        assert self.headers.get('Authorization') is None
        if self.mode == 'stall':
            time.sleep(5)
        payload = {'models': [{'name': name} for name in ['qwen3:8b', 'qwen2.5:7b', 'qwen3:8b', 'gpt-oss:120b-cloud', 'other:cloud']]}
        payload['models'].append({'name': 'remote-alias:latest', 'remote_host': 'https://ollama.com', 'remote_model': 'remote'})
        if self.mode == 'empty':
            payload = {'models': []}
        if self.mode == 'cloud':
            payload = {'models': [{'name': 'gpt-oss:120b-cloud'}]}
        if self.mode == 'wrong-server':
            payload = {'status': 'ok'}
        data = b'invalid JSON' if self.mode == 'invalid' else json.dumps(payload).encode()
        code = 404 if self.mode == 'missing' else 200
        try:
            self.send_response(code)
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass
    def do_POST(self):
        assert self.headers.get('Authorization') is None
        body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        self.seen.append((self.path, body))
        if self.mode == 'stall':
            time.sleep(5)
        code = {'auth': 401, 'quota': 429, 'redirect': 307, 'missing': 404, 'memory': 500}.get(self.mode, 200) if self.path != '/capture' else 200
        data = b'invalid JSON' if self.mode == 'invalid' else json.dumps({'error': 'model needs more system memory'} if self.mode == 'memory' else {'message': {'content': json.dumps(plan)}, 'done': True}).encode()
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
def quiet(stderr):
    # ALSA complains when /dev/snd is missing; that is not a test failure.
    return '\n'.join(line for line in stderr.splitlines() if 'ALSA lib' not in line and '/dev/snd/' not in line)
try:
    for mode, expected in [('ok', ''), ('auth', 'requires authentication'), ('quota', 'quota'), ('invalid', 'unreadable'), ('redirect', 'HTTP 307'), ('missing', 'not installed'), ('memory', 'more system memory')]:
        Handler.mode = mode
        result = subprocess.run([str(binary), '--ai-http-test', endpoint], capture_output=True, text=True, timeout=15)
        err = quiet(result.stderr)
        assert (result.returncode == 0) == (mode == 'ok'), (mode, err)
        assert expected in err, (mode, err)
    assert len(Handler.seen) == 7
    for path, body in Handler.seen:
        assert body['model'] == 'qwen3:8b' and body['think'] is False
        assert path == '/api/chat' and body['stream'] is False and body['format'] == 'json'
        context = json.loads(body['messages'][1]['content'])
        assert context['task'] == 'techno 6 minutes'
        assert [t['id'] for t in context['enabled_tracks']] == [4, 17]
    for mode, expected in [('ok', ''), ('empty', 'no local models'), ('cloud', 'no local models'), ('wrong-server', 'model list'), ('invalid', 'unreadable'), ('missing', 'API not found')]:
        Handler.mode = mode
        # Accept the frequently pasted /api endpoint, avoiding /api/api/tags.
        result = subprocess.run([str(binary), '--ai-models-test', endpoint + '/api'], capture_output=True, text=True, timeout=15)
        err = quiet(result.stderr)
        assert (result.returncode == 0) == (mode == 'ok'), (mode, err)
        assert expected in err, (mode, err)
    with socket.socket() as unused:
        unused.bind(('127.0.0.1', 0))
        closed_port = unused.getsockname()[1]
    result = subprocess.run([str(binary), '--ai-models-test', f'http://127.0.0.1:{closed_port}'], capture_output=True, text=True, timeout=15)
    assert result.returncode != 0 and 'Cannot reach Ollama' in quiet(result.stderr), quiet(result.stderr)
    Handler.mode = 'stall'
    start = time.monotonic()
    result = subprocess.run([str(binary), '--ai-models-cancel', endpoint], capture_output=True, text=True, timeout=8)
    assert result.returncode == 0 and time.monotonic() - start < 5, quiet(result.stderr)
    Handler.mode = 'stall'
    start = time.monotonic()
    result = subprocess.run([str(binary), '--ai-http-cancel', endpoint], capture_output=True, text=True, timeout=8)
    assert result.returncode == 0 and time.monotonic() - start < 5, quiet(result.stderr)
    if args.ui_output:
        Handler.mode = 'ok'
        app = root / args.build_dir / 'FaseGrooveArranger_artefacts' / 'Release' / 'Fase Groove Arranger'
        result = subprocess.run([str(app), '--ui-test', str(Path(args.ui_output).resolve())], env={**os.environ, 'FGA_AI_TEST_SERVER': endpoint}, capture_output=True, text=True, timeout=120)
        assert result.returncode == 0, quiet(result.stderr)[-4000:]
        for subdir in ['', 'ai-it']:
            evidence = Path(args.ui_output) / subdir / 'ai-local-http.txt'
            assert evidence.read_text().startswith('PASS'), evidence

finally:
    server.shutdown()
    server.server_close()
print('PASS Ollama HTTP fixtures (no inference): discovery, missing/empty/cloud models, memory errors, no-key requests, and request context, six-minute MIDI, authentication/quota/errors, blocked redirects and cancellable stalled response')
