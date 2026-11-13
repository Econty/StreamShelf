import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import http from 'node:http';
import test from 'node:test';

const executable = process.argv[2];
const start = server => new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const stop = server => new Promise(resolve => server.close(resolve));
const run = url => new Promise((resolve, reject) => {
  const child = spawn(executable, [url]);
  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', data => { stderr += data; });
  child.on('error', reject);
  child.on('close', code => code === 0 ? resolve() : reject(new Error(`exit ${code}: ${stderr}`)));
});

test('redirects retain same-origin authorization but drop sensitive cross-origin headers', async () => {
  const requests = [];
  const destination = http.createServer((req, res) => {
    requests.push({ stage: 'cross', headers: req.headers });
    res.end('OK');
  });
  await start(destination);
  const source = http.createServer((req, res) => {
    requests.push({ stage: req.url, headers: req.headers });
    if (req.url === '/same') res.writeHead(302, { Location: '/target' }).end();
    else if (req.url === '/cross') res.writeHead(302, { Location: `http://127.0.0.1:${destination.address().port}/target` }).end();
    else if (req.url === '/target' && req.headers.authorization !== 'Bearer synthetic-test-secret') res.writeHead(401).end('missing authorization');
    else res.end('OK');
  });
  await start(source);
  try {
    const base = `http://127.0.0.1:${source.address().port}`;
    await run(base + '/same');
    await run(base + '/cross');
    const first = requests.find(entry => entry.stage === '/same').headers;
    const same = requests.find(entry => entry.stage === '/target').headers;
    const cross = requests.find(entry => entry.stage === 'cross').headers;
    assert.equal(first.authorization, 'Bearer synthetic-test-secret');
    assert.equal(same['user-agent'], 'StreamShelf-Test-Agent');
    assert.equal(same.referer, 'https://page.example/watch');
    assert.equal(same.authorization, 'Bearer synthetic-test-secret');
    assert.equal(same.origin, 'https://page.example');
    assert.equal(cross['user-agent'], 'StreamShelf-Test-Agent');
    assert.equal(cross.referer, undefined);
    assert.equal(cross.authorization, undefined);
    assert.equal(cross.origin, undefined);
  } finally {
    await stop(source);
    await stop(destination);
  }
});
