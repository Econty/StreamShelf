"""Exercise App::download against a local server and an isolated data root."""
import http.server
import pathlib
import shutil
import subprocess
import sys
import threading

exe = pathlib.Path(sys.argv[1]).resolve()
test_dir = exe.parent.resolve()
data_dir = test_dir / 'preview-data'
if data_dir.exists():
    raise RuntimeError(f'Test data directory already exists: {data_dir}')

requests = []
class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        requests.append(self.path)
        body = b'CORRUPTED MEDIA THAT IS NOT HTML'
        self.send_response(200)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)
    def log_message(self, *args):
        pass

server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
try:
    run = subprocess.run([str(exe), f'http://127.0.0.1:{server.server_address[1]}/media.mp4'],
                         capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=30)
    if run.returncode or len(requests) != 2:
        raise AssertionError(f'exit={run.returncode}, requests={requests}, stdout={run.stdout}, stderr={run.stderr}')
    print('PASS: two retries issued two GETs and preserved both invalid files')
    previous_requests = len(requests)
    credentials = subprocess.run([str(exe), '--credential-download',
                                  f'http://127.0.0.1:{server.server_address[1]}/anonymous-fallback'],
                                 capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=15)
    if credentials.returncode or len(requests) != previous_requests:
        raise AssertionError(f'credential exit={credentials.returncode}, requests={requests}, stdout={credentials.stdout}, stderr={credentials.stderr}')
    print('PASS: partitioned browser credentials fail closed without an anonymous HTTP request')
    previous_requests = len(requests)
    names = subprocess.run([str(exe), '--filenames', f'http://127.0.0.1:{server.server_address[1]}'],
                           capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=20)
    if names.returncode or requests[previous_requests:] != ['/first', '/second']:
        raise AssertionError(f'names exit={names.returncode}, requests={requests}, stdout={names.stdout}, stderr={names.stderr}')
    print('PASS: preserved analyses retain their source titles in queue and output filenames')
    previews = subprocess.run([str(exe), '--preview-cache'], capture_output=True, text=True,
                              encoding='utf-8', errors='replace', timeout=15)
    if previews.returncode:
        raise AssertionError(f'previews exit={previews.returncode}, stdout={previews.stdout}, stderr={previews.stderr}')
    print('PASS: visible previews are stable and different browser sessions remain isolated')
    history = subprocess.run([str(exe), '--history'], capture_output=True, text=True,
                             encoding='utf-8', errors='replace', timeout=120)
    if history.returncode:
        raise AssertionError(f'history exit={history.returncode}, stdout={history.stdout}, stderr={history.stderr}')
    print('PASS: malformed and >64 MiB queues are recovered without overwriting their originals')
finally:
    server.shutdown()
    server.server_close()
    if data_dir.exists():
        resolved = data_dir.resolve()
        if resolved.parent != test_dir or resolved.name != 'preview-data' or data_dir.is_symlink():
            raise RuntimeError(f'Unsafe test cleanup path: {resolved}')
        shutil.rmtree(resolved)
