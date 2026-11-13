import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import fs from 'node:fs/promises';
import http from 'node:http';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

const executable = process.argv[2];
const run = (cwd, mode, url) => new Promise((resolve, reject) => {
  const child = spawn(executable, [mode, url], { cwd });
  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', chunk => { stderr += chunk; });
  child.on('error', reject);
  child.on('close', code => code === 0 ? resolve() : reject(new Error(`exit ${code}: ${stderr}`)));
});

test('resume validates the server response and restarts safely when required', async () => {
  const requests = [];
  const content = 'alpha-omega';
  const server = http.createServer((req, res) => {
    requests.push({ path: req.url, range: req.headers.range, ifRange: req.headers['if-range'] });
    if (req.url === '/redirect') return res.writeHead(302, { Location: '/target' }).end();
    if (req.url === '/interrupt' && !req.headers.range) {
      res.writeHead(200, { ETag: '"v1"', 'Content-Length': Buffer.byteLength(content) });
      res.write('alpha', () => setTimeout(() => res.destroy(), 100));
      return;
    }
    if (req.url === '/unsatisfied' && req.headers.range) return res.writeHead(416).end();
    if (req.headers.range && req.url !== '/changed') {
      const start = req.url === '/bad-range' ? 1 : 5;
      const body = content.slice(5);
      return res.writeHead(206, { ETag: '"v1"', 'Content-Range': `bytes ${start}-10/11`, 'Content-Length': body.length }).end(body);
    }
    const body = req.url === '/changed' ? 'different-content' : content;
    return res.writeHead(200, { ETag: '"v2"', 'Content-Length': Buffer.byteLength(body) }).end(body);
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const base = `http://127.0.0.1:${server.address().port}`;
  const tempRoot = await fs.realpath(os.tmpdir());
  const dirs = [];
  async function scenario(endpoint, expected) {
    const dir = await fs.mkdtemp(path.join(tempRoot, 'streamshelf-resume-test-'));
    dirs.push(dir);
    const url = base + endpoint;
    await run(dir, 'seed', url);
    await run(dir, 'fetch', url);
    assert.equal(await fs.readFile(path.join(dir, 'partial.part'), 'utf8'), expected);
    await assert.rejects(fs.stat(path.join(dir, 'partial.part.resume')), { code: 'ENOENT' });
  }
  try {
    await scenario('/ok', content);
    await scenario('/changed', 'different-content');
    await scenario('/bad-range', content);
    await scenario('/unsatisfied', content);
    await scenario('/redirect', content);
    const interrupted = await fs.mkdtemp(path.join(tempRoot, 'streamshelf-resume-test-'));
    dirs.push(interrupted);
    await assert.rejects(run(interrupted, 'fetch', base + '/interrupt'));
    assert.equal(await fs.readFile(path.join(interrupted, 'partial.part'), 'utf8'), 'alpha');
    await fs.stat(path.join(interrupted, 'partial.part.resume'));
    await run(interrupted, 'fetch', base + '/interrupt');
    assert.equal(await fs.readFile(path.join(interrupted, 'partial.part'), 'utf8'), content);
    await assert.rejects(fs.stat(path.join(interrupted, 'partial.part.resume')), { code: 'ENOENT' });
    const changedUrl = await fs.mkdtemp(path.join(tempRoot, 'streamshelf-resume-test-'));
    dirs.push(changedUrl);
    await run(changedUrl, 'seed', base + '/old-url');
    await run(changedUrl, 'fetch', base + '/new-url');
    assert.equal(await fs.readFile(path.join(changedUrl, 'partial.part'), 'utf8'), content);
    assert.equal(requests.find(req => req.path === '/new-url').range, undefined);
    const good = requests.find(req => req.path === '/ok');
    assert.equal(good.range, 'bytes=5-');
    assert.equal(good.ifRange, '"v1"');
    const bad = requests.filter(req => req.path === '/bad-range');
    assert.deepEqual(bad.map(req => req.range), ['bytes=5-', undefined]);
    const redirected = requests.find(req => req.path === '/target');
    assert.equal(redirected.range, undefined);
    assert.deepEqual(requests.filter(req => req.path === '/unsatisfied').map(req => req.range), ['bytes=5-', undefined]);
    assert.deepEqual(requests.filter(req => req.path === '/interrupt').map(req => req.range), [undefined, 'bytes=5-']);
  } finally {
    await new Promise(resolve => server.close(resolve));
    for (const dir of dirs) {
      if ((await fs.lstat(dir)).isSymbolicLink()) throw new Error('Refusing to clean a linked fixture directory');
      const resolved = await fs.realpath(dir);
      if (path.dirname(resolved).toLowerCase() !== tempRoot.toLowerCase() ||
          !path.basename(resolved).startsWith('streamshelf-resume-test-')) throw new Error('Unexpected fixture path');
      await fs.rm(resolved, { recursive: true, force: true });
    }
  }
});
