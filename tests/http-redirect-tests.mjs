import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import http from 'node:http';
import test from 'node:test';
import fs from 'node:fs';
import path from 'node:path';

const executable = process.argv[2];
const start = server => new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const stop = server => new Promise(resolve => server.close(resolve));
const run = (url, ...args) => new Promise((resolve, reject) => {
  const child = spawn(executable, [url, ...args]);
  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', data => { stderr += data; });
  child.on('error', reject);
  child.on('close', code => code === 0 ? resolve() : reject(new Error(`exit ${code}: ${stderr}`)));
});

test('authenticated media prefix is bounded and keeps credentials inside the HTTP client', async () => {
  let received;
  const source=http.createServer((req,res)=>{
    received=req.headers;
    if(req.headers.cookie!=='session=synthetic-test-cookie' || req.headers.authorization!=='Bearer synthetic-test-secret') {
      res.writeHead(403).end('denied');return;
    }
    res.setHeader('Content-Type','video/mp4');res.end('test-not-returned-to-decoder');
  });
  await start(source);
  try {
    await run(`http://127.0.0.1:${source.address().port}/video.mp4`,'prefix');
    assert.equal(received.cookie,'session=synthetic-test-cookie');
  } finally {await stop(source);}
});

const ffmpeg=path.join(process.env.LOCALAPPDATA||'', 'StreamShelfNative','tools','ffmpeg.exe');
test('authenticated video yields a decoded frame without decoder credential arguments', {skip:!fs.existsSync(ffmpeg)}, async () => {
  let authorized=false;
  const data=fs.readFileSync(new URL('./fixtures/preview-video.mp4',import.meta.url));
  const source=http.createServer((req,res)=>{
    authorized=req.headers.cookie==='session=synthetic-test-cookie' && req.headers.authorization==='Bearer synthetic-test-secret';
    if(!authorized){res.writeHead(403).end('denied');return;}
    res.setHeader('Content-Type','video/mp4');res.end(data);
  });
  await start(source);
  try {
    await run(`http://127.0.0.1:${source.address().port}/video.mp4`,'frame',ffmpeg);
    assert.equal(authorized,true);
  } finally {await stop(source);}
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
