import vm from 'node:vm';import fs from 'node:fs';import crypto from 'node:crypto';import assert from 'node:assert/strict';
let count=0;
for(const family of ['chromium','firefox']) {
 const manifest=JSON.parse(fs.readFileSync(`extension/${family==='firefox'?'firefox-development':'chromium-development'}/manifest.json`));
 if(family==='chromium') {
  const digest=crypto.createHash('sha256').update(Buffer.from(manifest.key,'base64')).digest('hex').slice(0,32);
  const id=[...digest].map(x=>String.fromCharCode(97+parseInt(x,16))).join('');
  assert.equal(id,'cjhiagnpgpmaagnoinjnkipimjlplcpe');count++;
  for(const size of [16,32,48,96,256]) {
   const image=fs.readFileSync(`extension/chromium-development/${manifest.icons[size]}`);
   assert.equal(image.subarray(0,8).toString('hex'),'89504e470d0a1a0a');
   assert.equal(image.readUInt32BE(16),size);assert.equal(image.readUInt32BE(20),size);
  }
  assert.equal(manifest.action.default_icon['32'],manifest.icons['32']);count+=16;
 }
 if(family==='firefox') {
  const gecko=manifest.browser_specific_settings.gecko;
  assert.equal(gecko.id,'connector@streamshelf.local');
  assert(Number(gecko.strict_min_version.split('.')[0])>=140);
  for(const required of ['browsingActivity','websiteContent','authenticationInfo']) assert(gecko.data_collection_permissions.required.includes(required));
  assert(manifest.permissions.includes('alarms'));
  assert.equal(manifest.version,'1.0.2');
  assert(manifest.description.length>20);
  for(const size of [16,32,48,96,256]) {
   const path=manifest.icons[size];const image=fs.readFileSync(`extension/firefox-development/${path}`);
   assert.equal(image.subarray(0,8).toString('hex'),'89504e470d0a1a0a');
   assert.equal(image.readUInt32BE(16),size);assert.equal(image.readUInt32BE(20),size);
  }
  assert.equal(manifest.action.default_icon['32'],manifest.icons['32']);
  count+=24;
 }
 let siteAccess=true;const stored={instanceId:'test'};const sent=[],listeners={};const event=key=>({addListener:f=>listeners[key]=f});
 const port={postMessage:m=>sent.push(m),onDisconnect:event('disconnect'),onMessage:event('native')};
 const api={runtime:{getManifest:()=>manifest,onMessage:event('message'),onInstalled:event('installed'),onStartup:event('startup'),connectNative:()=>port,lastError:null},storage:{local:{get:async key=>({[key]:stored[key]}),set:async values=>Object.assign(stored,values)}},tabs:{get:async id=>({id,url:id===99?'chrome://extensions/':'https://example.com/watch',title:'Test',...(family==='firefox'?{cookieStoreId:'firefox-container-1'}:{incognito:true})}),query:async()=>[{id:99}],reload:async()=>{},sendMessage:async()=>{},onActivated:event('activated'),onUpdated:event('updated'),onRemoved:event('removed')},windows:{WINDOW_ID_NONE:-1,get:async()=>({focused:true}),onFocusChanged:event('focus')},alarms:{create:()=>{},onAlarm:event('alarm')},permissions:{contains:async()=>siteAccess,onAdded:event('permissionAdded'),onRemoved:event('permissionRemoved')},scripting:{executeScript:async()=>{}},cookies:{getAllCookieStores:async()=>[{id:'private-store',tabIds:[7,8,9]}],getAll:async options=>{assert.equal(options.storeId,family==='firefox'?'firefox-container-1':'private-store');return [{domain:'example.com',name:'session',value:'SECRET',path:'/',secure:true,httpOnly:true,partitionKey:{topLevelSite:'https://example.com'}}];}},webRequest:{onBeforeSendHeaders:event('request'),onHeadersReceived:event('response'),onErrorOccurred:event('error')}};
 const context=vm.createContext({chrome:api,browser:family==='firefox'?api:undefined,URL,crypto:crypto.webcrypto,navigator:{userAgent:'Firefox/Test'},Map,Set,setTimeout,clearTimeout,console});
 vm.runInContext(fs.readFileSync(`extension/${family==='firefox'?'firefox-development':'chromium-development'}/background.js`,'utf8'),context);
 if(['chromium','firefox'].includes(family)){listeners.installed();await new Promise(resolve=>setTimeout(resolve,0));assert(sent.some(m=>m.type==='hello'&&m.allowLaunch===false));count++;}
 assert.equal(vm.runInContext("classify('https://e.com/file','video/mp4')",context),'Видео');count++;
 assert.equal(vm.runInContext('VERSION',context),manifest.version);count++;
 assert.equal(vm.runInContext("classify('https://e.com/segment.ts','video/mp2t')",context),'Сегмент');count++;
 assert.equal(vm.runInContext("classify('https://e.com/file','application/vnd.apple.mpegurl')",context),'HLS');count++;
 assert.equal(vm.runInContext("cleanUrl('file:///etc/passwd')",context),null);count++;
 if(['chromium','firefox'].includes(family)){assert.equal(vm.runInContext("classify('https://e.com/error.mp4','text/html')",context),null);count++;}
 if(['chromium','firefox'].includes(family)){siteAccess=false;const denied=await new Promise(resolve=>listeners.message({type:'connectExtension'},{},resolve));assert(denied.error);await vm.runInContext("handleCommand({type:'ping',requestId:'no-access'})",context);assert(sent.some(m=>m.type==='pong'&&m.requestId==='no-access'&&!m.hasSiteAccess&&!m.setupConfirmed));assert(!sent.some(m=>m.type==='setupChanged'));siteAccess=true;count+=3;}
 if(['chromium','firefox'].includes(family)){const result=await new Promise(resolve=>listeners.message({type:'connectExtension'},{},resolve));assert.equal(result.ok,true);assert.equal(vm.runInContext('tabId',context),null);listeners.activated({tabId:7});await new Promise(resolve=>setTimeout(resolve,0));count+=2;}else await new Promise(resolve=>listeners.message({type:'connectTab',tabId:7},{},resolve));
 await vm.runInContext("handleCommand({type:'ping',requestId:'challenge'})",context);assert(sent.some(m=>m.type==='pong'&&m.requestId==='challenge'&&((m.hasSiteAccess&&m.setupConfirmed))));if(['chromium','firefox'].includes(family)){assert(sent.some(m=>m.type==='setupChanged'));assert.equal(stored.setupConfirmed,true);assert(sent.findIndex(m=>m.type==='setupChanged')<sent.findIndex(m=>m.type==='pong'&&m.requestId==='challenge'));count+=3;}count++;
 if(family==='firefox'){stored.setupConfirmed=false;listeners.permissionAdded();await new Promise(resolve=>setTimeout(resolve,0));assert.equal(stored.setupConfirmed,true);assert(sent.some(m=>m.type==='setupChanged'));count+=2;}
 if(['chromium','firefox'].includes(family)){const changes=sent.filter(m=>m.type==='setupChanged').length;siteAccess=false;listeners.permissionRemoved();assert.equal(sent.filter(m=>m.type==='setupChanged').length,changes+1);await vm.runInContext("handleCommand({type:'ping',requestId:'revoked'})",context);assert(sent.some(m=>m.type==='pong'&&m.requestId==='revoked'&&!m.hasSiteAccess&&m.setupConfirmed));siteAccess=true;count+=2;}
 if(['chromium','firefox'].includes(family)){listeners.activated({tabId:8});await new Promise(resolve=>setTimeout(resolve,0));assert(sent.some(m=>m.type==='tabSelected'&&m.tabId===8));listeners.focus(-1);await new Promise(resolve=>setTimeout(resolve,0));assert.equal(vm.runInContext('tabId',context),8);listeners.activated({tabId:7});await new Promise(resolve=>setTimeout(resolve,0));count+=2;}
 await vm.runInContext("handleCommand({type:'scanStart',tabId:7})",context);
 listeners.response({tabId:99,url:'https://foreign.example/secret.mp4',requestId:'x',responseHeaders:[{name:'Content-Type',value:'video/mp4'}]});
 assert(!sent.some(m=>m.item?.url?.includes('foreign')));count++;
 listeners.request({tabId:7,url:'https://example.com/media.mp4?token=SIGNED',requestId:'ok',requestHeaders:[{name:'Authorization',value:'Bearer SECRET'}]});
 listeners.response({tabId:7,url:'https://example.com/media.mp4?token=SIGNED',requestId:'ok',responseHeaders:[{name:'Content-Type',value:'video/mp4'}]});
 await new Promise(resolve=>setTimeout(resolve,0));
 assert(!sent.some(m=>m.type==='scanCredentials'));count++;
 const found=sent.find(m=>m.type==='resource');assert.equal(found.item.url,'https://example.com/media.mp4?token=SIGNED');assert(!JSON.stringify(found).includes('SECRET'));count++;
 await vm.runInContext("handleCommand({type:'credentials',requestId:'r',tabId:7,url:'https://example.com/media.mp4?token=SIGNED'})",context);
 const creds=sent.find(m=>m.type==='credentialsResult');assert.equal(creds.cookies[0].partitioned,true);assert.equal(creds.headers.Authorization,'Bearer SECRET');count++;
 await vm.runInContext("handleCommand({type:'credentials',requestId:'canonical',tabId:7,url:'https://EXAMPLE.COM/watch'})",context);assert.equal(sent.find(m=>m.requestId==='canonical').url,'https://EXAMPLE.COM/watch');count++;
 assert.equal(vm.runInContext("classify('https://e.com/license.mp4','video/mp4')",context),'Видео');count++;
 listeners.request({tabId:7,url:'https://example.com/favicon.ico',requestId:'favicon',requestHeaders:[]});const notices=sent.filter(m=>m.type==='notice').length;listeners.response({tabId:7,url:'https://example.com/favicon.ico',requestId:'favicon',statusCode:404,responseHeaders:[]});assert.equal(sent.filter(m=>m.type==='notice').length,notices);count++;
 await assert.rejects(vm.runInContext("handleCommand({type:'credentials',requestId:'bad',tabId:7,url:'https://not-observed.example/'})",context));count++;
 await vm.runInContext("handleCommand({type:'scanStop'})",context);
 const before=sent.filter(m=>m.type==='resource').length;
 listeners.response({tabId:7,url:'https://example.com/new.mp4',requestId:'no',responseHeaders:[{name:'Content-Type',value:'video/mp4'}]});
 assert.equal(sent.filter(m=>m.type==='resource').length,before);count++;
 if(['chromium','firefox'].includes(family)){
  await vm.runInContext("handleCommand({type:'scanStart',tabId:7,scanId:2})",context);
  listeners.message({type:'dom',scanId:1,items:[{url:'https://example.com/stale.mp4',mime:'video/mp4'}]},{tab:{id:7}},()=>{});
  assert(!sent.some(m=>m.item?.url?.includes('stale.mp4')));count++;
  await vm.runInContext("handleCommand({type:'scanStop',scanId:1})",context);
  assert.equal(vm.runInContext('scanning',context),true);count++;
  listeners.request({tabId:7,url:'https://example.com/denied.mp4',requestId:'denied',requestHeaders:[]});
  listeners.response({tabId:7,url:'https://example.com/denied.mp4',requestId:'denied',statusCode:403,responseHeaders:[{name:'Content-Type',value:'video/mp4'}]});
  assert(!sent.some(m=>m.item?.url?.includes('denied.mp4')));assert(sent.some(m=>m.type==='notice'&&m.message.includes('403')));count+=2;
  listeners.request({tabId:7,url:'https://example.com/old.mp4',requestId:'old',requestHeaders:[]});
  await vm.runInContext("handleCommand({type:'scanStart',tabId:7,scanId:3})",context);
  listeners.response({tabId:7,url:'https://example.com/old.mp4',requestId:'old',statusCode:200,responseHeaders:[{name:'Content-Type',value:'video/mp4'}]});
  assert(!sent.some(m=>m.item?.url?.includes('old.mp4')));count++;
  listeners.activated({tabId:8});await new Promise(resolve=>setTimeout(resolve,0));
  assert(sent.some(m=>m.type==='scanState'&&m.scanId===3&&m.scanning===false));assert.equal(vm.runInContext('scanning',context),false);count+=2;
  listeners.activated({tabId:99});await new Promise(resolve=>setTimeout(resolve,0));
  assert.equal(vm.runInContext('tabId',context),null);assert.equal(vm.runInContext('pageUrl',context),'');
  assert.equal(vm.runInContext('resources.size',context),0);assert.equal(sent.at(-1).type,'tabClosed');count+=4;
  listeners.activated({tabId:7});await new Promise(resolve=>setTimeout(resolve,0));
  listeners.updated(9,{url:'https://example.com/watch'},{active:true,windowId:1});await new Promise(resolve=>setTimeout(resolve,0));
  assert.equal(vm.runInContext('tabId',context),9);count++;
 }
 console.log(`PASS ${family}: URL classification, tab scope, stop, signed URL, cookie partition metadata, private headers`);
}
console.log(`${count} extension checks passed`);

// Firefox's permission prompt must not stay behind its action popup.
{
 const popupCode=fs.readFileSync('extension/firefox-development/popup.js','utf8');
 for(const preGranted of [false,true]) {
  let closed=0,requestCount=0,connectCount=0;
  const button={disabled:false,onclick:null},status={textContent:''},win={close(){closed++;}};
  const api={permissions:{contains:async()=>preGranted,request:()=>{requestCount++;return Promise.resolve(true);}},
   extension:{getViews:()=>[win]},runtime:{getManifest:()=>({version:'1.0.2'}),sendMessage:async m=>{if(m.type==='connectExtension'){connectCount++;return {ok:true};}return {connected:false};}}};
  const context=vm.createContext({browser:api,window:win,document:{querySelector:s=>s==='#connect'?button:status},Promise});
  vm.runInContext(popupCode,context);
  await new Promise(resolve=>setTimeout(resolve,0));
  await button.onclick();
  assert.equal(requestCount,preGranted?0:1);
  assert.equal(closed,preGranted?0:1);
  assert.equal(connectCount,1);
 }
 console.log('PASS Firefox popup requests permission before closing; existing permission connects without closing');
}
