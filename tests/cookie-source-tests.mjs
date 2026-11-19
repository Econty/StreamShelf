import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import fs from 'node:fs';
import vm from 'node:vm';

for (const family of ['chromium', 'firefox']) {
  let account = 'A', expired = false, readPause = null, tabGetHook = null;
  const sent = [], listeners = {};
  const event = name => ({addListener: callback => {listeners[name] = callback;}});
  const port = {postMessage: message => sent.push(message), onDisconnect: event('disconnect'), onMessage: event('native')};
  const tab = {id: 7, url: 'https://account.example/watch', ...(family === 'firefox' ? {cookieStoreId:'same-store'} : {})};
  const api = {
    runtime: {getManifest: () => ({version: '1.0.6'}), connectNative: () => port, onMessage: event('message'), onInstalled: event('installed'), onStartup: event('startup')},
    storage: {local: {get: async () => ({instanceId: 'test'}), set: async () => {}}},
    tabs: {get: async () => {if(tabGetHook)await tabGetHook();return tab;}, query: async () => [], sendMessage: async () => {}, onActivated: event('activated'), onUpdated: event('updated'), onRemoved: event('removed')},
    windows: {onFocusChanged: event('focus')}, alarms: {onAlarm: event('alarm'), create() {}},
    permissions: {onAdded: event('added'), onRemoved: event('removed')}, scripting: {executeScript: async () => {}},
    webRequest: {onBeforeSendHeaders: event('request'), onHeadersReceived: event('response'), onErrorOccurred: event('error')},
    cookies: {onChanged: event('cookieChanged'), getAllCookieStores: async () => [{id:'same-store',tabIds:[7]}], getAll: async options => {
      assert.equal(options.storeId,'same-store');
      if(readPause)await readPause;
      return expired ? [] : [{domain:'account.example', name:'session', value:account, path:'/', expirationDate:9999999999}];
    }}
  };
  const context = vm.createContext({browser: family === 'firefox' ? api : undefined, chrome: api, URL, crypto: crypto.webcrypto, navigator: {userAgent: 'test'}, Map, Set, setTimeout, clearTimeout});
  vm.runInContext(fs.readFileSync(`extension/${family}-development/background.js`, 'utf8'), context);
  const run = code => vm.runInContext(code,context);
  const value = name => run(name);
  const settle = () => new Promise(resolve=>setTimeout(resolve,50));
  const request = (id,revision=value('credentialRevision')) => run(`handleCommand(${JSON.stringify({type:'credentials',requestId:id,tabId:7,sourceId:value('sourceId'),credentialRevision:revision,storeId:tab.cookieStoreId||null,url:tab.url})})`);
  await run('connect()');await run('selectActiveTab(7)');
  const source=value('sourceId');
  await run("handleCommand({type:'scanStart',tabId:7,scanId:1})");
  run("found('https://account.example/video.mp4','video/mp4')");
  await request('original');

  // Returning focus to the same tab must not cancel an in-flight cookie read.
  let releaseRead;readPause=new Promise(resolve=>{releaseRead=resolve;});
  const pending=request('same-tab-focus');
  await Promise.resolve();
  await run('selectActiveTab(7)');
  readPause=null;releaseRead();await pending;
  assert.equal(value('sourceId'),source);
  assert(sent.some(m=>m.requestId==='same-tab-focus'&&m.type==='credentialsResult'));

  const initialRevision=value('credentialRevision');
  listeners.cookieChanged({removed:true,cause:'overwrite',cookie:{domain:'account.example',storeId:'same-store',path:'/'}});
  listeners.cookieChanged({removed:false,cause:'explicit',cookie:{domain:'account.example',storeId:'same-store',path:'/'}});
  await settle();
  assert.equal(value('credentialRevision'),initialRevision,'same-value cookie renewal keeps the credential revision');

  for (const cookie of [
    {domain:'unrelated.example',path:'/'},
    {domain:'account.example',storeId:'other-store',path:'/'},
    {domain:'account.example',storeId:'same-store',path:'/private'},
    {domain:'account.example',storeId:'same-store',path:'/wat'},
    {domain:'account.example',storeId:'same-store',path:'/',partitionKey:{topLevelSite:'https://other.example'}}
  ]) listeners.cookieChanged({cookie});
  await settle();
  assert.equal(value('credentialRevision'),initialRevision,'other stores, paths and partitions cannot invalidate credentials');
  assert.equal(run("relevantCookie({domain:'example',hostOnly:true,path:'/'})"),false);
  assert.equal(run("relevantCookie({domain:'.example',hostOnly:false,path:'/'})"),true);
  assert.equal(run("relevantCookie({domain:'account.example',storeId:'same-store',path:'/video.mp4'})"),true);

  account='B'; // Detect a change before the browser delivers cookies.onChanged too.
  await assert.rejects(request('stale',initialRevision),/refreshed|обновились/i);
  assert.equal(value('sourceId'),source,'authentication refresh is not navigation');
  assert.equal(value('scanning'),true,'cookie changes do not stop discovery');
  assert.equal(value('resources.size'),1,'cookie changes do not discard discovered media');
  await request('fresh');
  const fresh=sent.find(m=>m.requestId==='fresh');
  assert.deepEqual(fresh.cookies.map(c=>c.value),['B'],'new snapshots never mix old account cookies');
  assert(fresh.credentialRevision>initialRevision);
  const changed=sent.filter(m=>m.type==='credentialsChanged').at(-1);
  assert.equal(changed.storeId,tab.cookieStoreId||null,'event store identity matches tabSelected/credentialsResult');
  assert.equal(changed.sourceId,source);
  assert(!JSON.stringify(changed).includes('session'),'change events contain no cookie contents');
  const rediscoveries=sent.filter(m=>m.type==='resource').length;
  run("found('https://account.example/video.mp4','video/mp4')");
  assert.equal(sent.filter(m=>m.type==='resource').length,rediscoveries+1,'observed resources can be rediscovered under the refreshed credentials');
  assert.equal(sent.filter(m=>m.type==='resource').at(-1).credentialRevision,value('credentialRevision'));
  run("found('https://account.example/video.mp4','video/mp4')");
  assert.equal(sent.filter(m=>m.type==='resource').length,rediscoveries+1,'unchanged rediscovery in the same revision remains deduplicated');

  const beforeRemoval=value('credentialRevision');expired=true;
  listeners.cookieChanged({removed:true,cause:'expired',cookie:{domain:'account.example',storeId:'same-store',path:'/'}});
  await settle();
  assert(value('credentialRevision')>beforeRemoval,'deleted authentication cookies invalidate reusable snapshots');
  assert.equal(value('sourceId'),source);assert.equal(value('scanning'),true);
  await request('removed');
  assert.equal(sent.find(m=>m.requestId==='removed').cookies.length,0,'removed cookies are not silently resurrected');

  const media='https://account.example/video.mp4';
  listeners.request({tabId:7,url:media,requestId:'first',requestHeaders:[{name:'Authorization',value:'Bearer A'}]});
  listeners.response({tabId:7,url:media,requestId:'first',responseHeaders:[{name:'Content-Type',value:'video/mp4'}]});
  const beforeAuthChange=value('credentialRevision');
  listeners.request({tabId:7,url:media,requestId:'second',requestHeaders:[{name:'Authorization',value:'Bearer B'}]});
  assert(value('credentialRevision')>beforeAuthChange);
  assert.equal(value('sourceId'),source);assert.equal(value('scanning'),true);assert.equal(value('resources.size'),1);
  await run(`handleCommand(${JSON.stringify({type:'credentials',requestId:'auth-fresh',tabId:7,sourceId:source,credentialRevision:value('credentialRevision'),url:media})})`);
  assert.equal(sent.find(m=>m.requestId==='auth-fresh').headers.Authorization,'Bearer B','no stale Authorization between request and response');
  expired=false;account='C';
  const beforeCookieRotationEvents=sent.filter(m=>m.type==='resource').length;
  listeners.cookieChanged({removed:false,cause:'explicit',cookie:{domain:'account.example',storeId:'same-store',path:'/'}});
  await settle();
  assert.equal(sent.filter(m=>m.type==='resource').length,beforeCookieRotationEvents,'credential changes do not replay the old resource list');
  await run(`handleCommand(${JSON.stringify({type:'credentials',requestId:'auth-cookie-fresh',tabId:7,sourceId:source,credentialRevision:value('credentialRevision'),url:media})})`);
  assert.equal(sent.find(m=>m.requestId==='auth-cookie-fresh').headers.Authorization,undefined,'cookies from the new account are never combined with a cached Authorization from the old account');
  listeners.request({tabId:7,url:media,requestId:'pending-old',requestHeaders:[{name:'Authorization',value:'Bearer C'}]});
  const pendingRevision=value('credentialRevision');account='D';
  listeners.cookieChanged({removed:false,cause:'explicit',cookie:{domain:'account.example',storeId:'same-store',path:'/'}});
  await settle();
  listeners.response({tabId:7,url:media,requestId:'pending-old',responseHeaders:[{name:'Content-Type',value:'video/mp4'}]});
  assert.equal(run(`resources.get(${JSON.stringify(media)}).headers.Authorization`),undefined,'late responses cannot restore an old bearer token');
  assert.equal(sent.filter(m=>m.type==='resource').at(-1).credentialRevision,pendingRevision,'an old request is not promoted into the new account revision');
  run(`found(${JSON.stringify(media)},'video/mp4')`);
  const observedCurrent=value('credentialRevision');
  listeners.request({tabId:7,url:media,requestId:'pending-older',requestHeaders:[]});
  account='E';listeners.cookieChanged({removed:false,cause:'explicit',cookie:{domain:'account.example',storeId:'same-store',path:'/'}});await settle();
  run(`found(${JSON.stringify(media)},'video/mp4')`);
  listeners.response({tabId:7,url:media,requestId:'pending-older',responseHeaders:[{name:'Content-Type',value:'video/mp4'}]});
  assert(value('credentialRevision')>observedCurrent);
  assert.equal(run(`resources.get(${JSON.stringify(media)}).discoveryRevision`),value('credentialRevision'),'late responses do not downgrade a newer observed resource');

  let tabReads=0;
  tabGetHook=()=>{if(++tabReads===2)run('noteCredentialChange()');};
  await assert.rejects(request('final-revision-race'),/refreshed|обновились/i,'the final asynchronous tab check cannot hide credential changes');
  assert(!sent.some(m=>m.requestId==='final-revision-race'));
  tabReads=0;
  tabGetHook=()=>{if(++tabReads===2)listeners.cookieChanged({cookie:{domain:'account.example',storeId:'same-store',path:'/'}});};
  await assert.rejects(request('final-cookie-race'),/refreshed|обновились/i,'even a not-yet-debounced cookie event invalidates a concurrent read');
  tabGetHook=null;await settle();

  run("found('https://cdn.example/assets/video.mp4','video/mp4')");
  const beforeCdnChange=value('credentialRevision');
  listeners.cookieChanged({removed:true,cause:'expired',cookie:{domain:'cdn.example',storeId:'same-store',path:'/assets'}});
  assert(value('credentialRevision')>beforeCdnChange,'resource-only cache is revoked before the debounce delay');
  await settle();
  assert(value('credentialRevision')>beforeCdnChange,'resource-only credentials invalidate cache even when the page jar is unchanged');
  assert.equal(value('sourceId'),source);assert.equal(value('resources.size'),2);assert.equal(value('scanning'),true);

  listeners.request({tabId:7,url:media,requestId:'old-document',requestHeaders:[{name:'Authorization',value:'Bearer old-document'}]});
  listeners.updated(7,{status:'loading'},tab);
  assert(value('sourceId')>source,'real same-URL reloads still change document identity');
  assert.equal(value('resources.size'),0,'reload discards the previous document resource identities');
  assert.equal(value('requests.size'),0,'reload discards pending requests from the previous document');
  assert.equal(value('scanning'),true,'scanStart reload keeps discovery running');
  assert.equal(value('credentialRevision'),0);
  await assert.rejects(run(`handleCommand(${JSON.stringify({type:'credentials',requestId:'old-resource',tabId:7,sourceId:value('sourceId'),credentialRevision:0,url:media})})`),/selected tab|выбранной вкладке/i);
  run(`found(${JSON.stringify(media)},'video/mp4')`);
  assert.equal(value('resources.size'),1,'same media can be discovered under the new document revision');
  assert.equal(sent.filter(m=>m.type==='resource').at(-1).credentialRevision,0);
  listeners.response({tabId:7,url:media,requestId:'old-document',responseHeaders:[{name:'Content-Type',value:'video/mp4'}]});
  assert.equal(run(`resources.get(${JSON.stringify(media)}).headers?.Authorization`),undefined,'old-document responses cannot restore credentials');
  console.log(`PASS ${family}: focus/renewal preserve discovery; account/removal/header/CDN changes revise credentials separately`);
}
