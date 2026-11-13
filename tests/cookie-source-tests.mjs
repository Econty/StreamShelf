import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import fs from 'node:fs';
import vm from 'node:vm';

for (const family of ['chromium', 'firefox']) {
  let account = 'A';
  const sent = [], listeners = {};
  const event = name => ({addListener: callback => {listeners[name] = callback;}});
  const port = {postMessage: message => sent.push(message), onDisconnect: event('disconnect'), onMessage: event('native')};
  const tab = {id: 7, url: 'https://account.example/watch', cookieStoreId: 'same-store'};
  const api = {
    runtime: {getManifest: () => ({version: '1.0.5'}), connectNative: () => port, onMessage: event('message'), onInstalled: event('installed'), onStartup: event('startup')},
    storage: {local: {get: async () => ({instanceId: 'test'}), set: async () => {}}},
    tabs: {get: async () => tab, query: async () => [], sendMessage: async () => {}, onActivated: event('activated'), onUpdated: event('updated'), onRemoved: event('removed')},
    windows: {onFocusChanged: event('focus')}, alarms: {onAlarm: event('alarm'), create() {}},
    permissions: {onAdded: event('added'), onRemoved: event('removed')}, scripting: {executeScript: async () => {}},
    webRequest: {onBeforeSendHeaders: event('request'), onHeadersReceived: event('response'), onErrorOccurred: event('error')},
    cookies: {onChanged: event('cookieChanged'), getAll: async () => [{domain: 'account.example', name: 'session', value: account, path: '/'}]}
  };
  const context = vm.createContext({browser: family === 'firefox' ? api : undefined, chrome: api, URL, crypto: crypto.webcrypto, navigator: {userAgent: 'test'}, Map, Set, setTimeout, clearTimeout});
  vm.runInContext(fs.readFileSync(`extension/${family}-development/background.js`, 'utf8'), context);
  await vm.runInContext('connect()', context);
  await vm.runInContext('selectActiveTab(7)', context);
  const source = vm.runInContext('sourceId', context);
  const command = `handleCommand({type:'credentials',requestId:'test',tabId:7,sourceId:${source},storeId:'same-store',url:'https://account.example/watch'})`;
  await vm.runInContext(command, context);
  account = 'B'; // No navigation, and deliberately do not fire onChanged yet.
  await assert.rejects(vm.runInContext(command, context), /changed|изменил/i);
  assert.deepEqual(sent.filter(m => m.type === 'credentialsResult').map(m => m.cookies[0].value), ['A']);
  await new Promise(resolve => setTimeout(resolve, 0));
  const current = vm.runInContext('sourceId', context);
  listeners.cookieChanged({cookie: {domain: 'unrelated.example'}});
  assert.equal(vm.runInContext('sourceId', context), current);
  await vm.runInContext("handleCommand({type:'scanStart',tabId:7,scanId:1})", context);
  vm.runInContext("found('https://account.example/video.mp4','video/mp4')", context);
  for (const cookie of [
    {domain: 'account.example', storeId: 'other-store', path: '/'},
    {domain: 'account.example', storeId: 'same-store', path: '/private'},
    // RFC path matching requires a slash boundary: /wat does not match /watch.
    {domain: 'account.example', storeId: 'same-store', path: '/wat'},
    {domain: 'account.example', storeId: 'same-store', path: '/', partitionKey: {topLevelSite: 'https://other.example'}}
  ]) {
    listeners.cookieChanged({cookie});
    assert.equal(vm.runInContext('sourceId', context), current, 'unrelated cookie keeps the source identity');
    assert.equal(vm.runInContext('scanning', context), true, 'unrelated cookie does not stop discovery');
    assert.equal(vm.runInContext('resources.size', context), 1, 'unrelated cookie does not discard resources');
  }
  assert.equal(vm.runInContext("relevantCookie({domain:'example',hostOnly:true,path:'/'})", context), false, 'host-only cookies do not apply to a subdomain');
  assert.equal(vm.runInContext("relevantCookie({domain:'.example',hostOnly:false,path:'/'})", context), true, 'domain cookies apply to subdomains');
  assert.equal(vm.runInContext("relevantCookie({domain:'account.example',storeId:'same-store',path:'/video.mp4'})", context), true, 'resource paths are considered too');
  account = 'C';
  listeners.cookieChanged({cookie: {domain: '.account.example', storeId: 'same-store', path: '/watch', partitionKey: {topLevelSite: 'https://account.example'}}});
  assert(vm.runInContext('sourceId', context) > current);
  assert.equal(vm.runInContext('scanning', context), false, 'a relevant change invalidates the scan');
  await new Promise(resolve => setTimeout(resolve, 0));
  await vm.runInContext("handleCommand({type:'scanStart',tabId:7,scanId:2})", context);
  const media = 'https://account.example/video.mp4';
  listeners.request({tabId:7,url:media,requestId:'first',requestHeaders:[{name:'Authorization',value:'Bearer A'}]});
  listeners.response({tabId:7,url:media,requestId:'first',responseHeaders:[{name:'Content-Type',value:'video/mp4'}]});
  const beforeAuthChange = vm.runInContext('sourceId', context);
  listeners.request({tabId:7,url:media,requestId:'second',requestHeaders:[{name:'Authorization',value:'Bearer B'}]});
  assert(vm.runInContext('sourceId', context) > beforeAuthChange, 'changed authorization invalidates the source');
  console.log(`PASS ${family}: account changes invalidate the original source`);
}
