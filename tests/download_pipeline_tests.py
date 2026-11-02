"""Exercise the actual local yt-dlp/FFmpeg pipeline, without a browser or external network."""
import functools
import http.server
import json
import os
import pathlib
import subprocess
import tempfile
import threading

tools = pathlib.Path(os.environ['LOCALAPPDATA']) / 'StreamShelfNative' / 'tools'
ffmpeg, ffprobe, ytdlp = (tools / name for name in ('ffmpeg.exe', 'ffprobe.exe', 'yt-dlp.exe'))

def run(args, timeout=45):
    return subprocess.run([str(x) for x in args], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          text=True, encoding='utf-8', errors='replace', timeout=timeout, check=True)

with tempfile.TemporaryDirectory(prefix='streamshelf-pipeline-') as temp:
    root = pathlib.Path(temp)
    run([ffmpeg, '-hide_banner', '-loglevel', 'error', '-f', 'lavfi', '-i', 'color=c=black:s=64x64:r=2:d=1',
         '-c:v', 'mpeg4', '-an', '-y', root / 'video.mp4'])
    run([ffmpeg, '-hide_banner', '-loglevel', 'error', '-f', 'lavfi', '-i', 'sine=frequency=440:duration=1',
         '-c:a', 'aac', '-vn', '-y', root / 'audio.m4a'])
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=str(root))
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        base = f'http://127.0.0.1:{server.server_address[1]}'
        info = {'id': 'fixture', 'title': 'Fixture', 'extractor': 'generic', 'extractor_key': 'Generic',
                'webpage_url': base + '/fixture', 'formats': [
                    {'format_id': 'v', 'url': base + '/video.mp4', 'ext': 'mp4', 'vcodec': 'mpeg4', 'acodec': 'none'},
                    {'format_id': 'a', 'url': base + '/audio.m4a', 'ext': 'm4a', 'vcodec': 'none', 'acodec': 'aac'}]}
        info_path = root / 'fixture.info.json'
        info_path.write_text(json.dumps(info), encoding='utf-8')
        output = root / 'result'
        output.mkdir()
        result = run([ytdlp, '--ignore-config', '--encoding', 'utf-8', '--no-playlist', '--newline',
                      '--no-simulate', '--print', 'after_move:STREAMSHELF_FINAL:%(filepath)s',
                      '--ffmpeg-location', tools, '--load-info-json', info_path, '-f', 'v+a',
                      '--merge-output-format', 'mkv', '-o', output / 'media.%(ext)s'])
        markers = [line.split('STREAMSHELF_FINAL:', 1)[1] for line in result.stdout.splitlines()
                   if line.startswith('STREAMSHELF_FINAL:')]
        assert len(markers) == 1, (markers, result.stdout, result.stderr)
        final = pathlib.Path(markers[0])
        assert final.exists() and final.parent.resolve() == output.resolve(), final
        probe = json.loads(run([ffprobe, '-v', 'error', '-show_entries', 'stream=codec_type', '-of', 'json', final]).stdout)
        assert {'audio', 'video'} <= {stream.get('codec_type') for stream in probe['streams']}, probe
        assert len(list(output.iterdir())) == 1, list(output.iterdir())
        print('PASS yt-dlp merges separate audio and video and identifies exactly the final file')
        html = root / 'error.mp4'
        html.write_text('<html><body>Forbidden</body></html>', encoding='utf-8')
        invalid = subprocess.run([str(ffprobe), '-v', 'quiet', '-show_entries', 'stream=codec_type', '-of', 'json', str(html)],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10)
        assert invalid.returncode != 0
        print('PASS ffprobe rejects HTML disguised as a media file')
    finally:
        server.shutdown()
        server.server_close()
