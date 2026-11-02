'use strict';
const api = globalThis.browser || chrome;
const VERSION = '1.0.0';
let port = null, tabId = null, scanning = false, pageUrl = '', instanceId = '', lastError = '';
let connecting = null;
let selectionSequence = 0, scanId = 0;
const resources = new Map(), requests = new Map(), segments = new Map();let pageHeaders={};
function send(message) { if(port) { try { port.postMessage(message); } catch { lastError='Связь с приложением потеряна'; } } }
function cleanUrl(value) { try { const u=new URL(value); return ['http:','https:'].includes(u.protocol)&&!u.username&&!u.password?u.href:null; } catch {return null;} }
function classify(url,mime='') {
  let path,u;try {u=new URL(url);path=u.pathname.toLowerCase();}catch{return null;}
  mime=String(mime).toLowerCase().split(';')[0].trim();
  if(/^(text\/html|text\/css|(?:application|text)\/(?:json|javascript|xml)|font\/)/.test(mime))return null;
  if(!mime||mime==='application/octet-stream'){
    const declared=u.searchParams.get('mime')||u.searchParams.get('type')||'';
    if(/^(video|audio)\/[a-z0-9.+-]+$/i.test(declared)||/^(application\/(vnd\.apple\.mpegurl|x-mpegurl|dash\+xml))$/i.test(declared))mime=declared.toLowerCase();
  }
  if(/widevine|playready|license|licence/.test(path)&&!mime.startsWith('image/'))return 'DRM / лицензия · кандидат';
  if(path.endsWith('.m3u8')||mime.includes('mpegurl'))return 'HLS';
  if(path.endsWith('.mpd')||mime.includes('dash+xml'))return 'DASH';
  if(/\.(m4s|ts|cmfv|cmfa)$/.test(path))return 'Сегмент';
  if(mime.startsWith('video/')||/\.(mp4|webm|mkv|mov|ogv)$/.test(path))return 'Видео';
  if(mime.startsWith('audio/')||/\.(m4a|mp3|aac|opus|ogg|flac|wav)$/.test(path))return 'Аудио';
  if(/\.(m4s|ts|cmfv|cmfa)$/.test(path))return 'Сегмент';
  if(mime.includes('vtt')||mime.includes('ttml')||/\.(vtt|srt|ttml)$/.test(path))return 'Субтитры';
  if(mime.startsWith('image/')||/\.(jpe?g|png|webp|avif)$/.test(path))return 'Изображения';
  return null;
}
function found(url,mime='',size=null,source='Сеть',drm=false,thumbnail='',title='') {
  url=cleanUrl(url);if(!url||url.length>16000)return;
  const kind=classify(url,mime);if(!kind)return;if(kind==='Сегмент'){const u=new URL(url);const group=u.origin+u.pathname.slice(0,u.pathname.lastIndexOf('/')+1);const n=(segments.get(group)||0)+1;segments.set(group,n);if(n===1||n%20===0)send({type:'notice',message:`Обнаружены сегменты потока: ${n}. Ищите исходный HLS/DASH-манифест.`});return;}
  if(resources.size>=1000&&!resources.has(url)){
    if(kind==='Изображения')return;
    const image=[...resources].find(([,item])=>item.kind==='Изображения');
    if(image)resources.delete(image[0]);else return;
  }
  const old=resources.get(url);const item={url,mime:mime||old?.mime||'',kind,size:size??old?.size??null,source,drm:drm||old?.drm||kind.startsWith('DRM'),thumbnail:cleanUrl(thumbnail)||old?.thumbnail||'',title:title||old?.title||'',time:Date.now()};
  resources.set(url,{...old,...item});
  if(!old||old.mime!==item.mime||old.size!==item.size||old.thumbnail!==item.thumbnail||old.title!==item.title)send({type:'resource',scanId,tabId,pageUrl,item});
}
async function connect(allowLaunch=true) {
  if(port)return;
  if(connecting)return connecting;
  connecting=(async()=>{
  const stored=await api.storage.local.get('instanceId');instanceId=stored.instanceId||crypto.randomUUID();await api.storage.local.set({instanceId});
  const opened=api.runtime.connectNative('dev.streamshelf.connector');port=opened;
  opened.onDisconnect.addListener(()=>{if(port!==opened)return;lastError=api.runtime.lastError?.message||'Приложение отключено';port=null;scanning=false;requests.clear();resources.clear();});
  opened.onMessage.addListener(message=>{handleCommand(message).catch(e=>send({type:'error',requestId:message.requestId,message:e.message}));});
  send({type:'hello',protocol:1,version:VERSION,instanceId,browser:navigator.userAgent,allowLaunch,capabilities:['network','dom','cookies','tabReload']});
  if(tabId===null){const tabs=await api.tabs.query({active:true,lastFocusedWindow:true}).catch(()=>[]);if(tabs[0])await selectActiveTab(tabs[0].id);}
  if(tabId!==null)await announce();
  })();
  try { await connecting; } finally { connecting=null; }
}
async function announce() {
  if(tabId===null)return;const t=await api.tabs.get(tabId);pageUrl=cleanUrl(t.url)||'';
  send({type:'tabSelected',tabId,url:pageUrl,title:t.title||'',storeId:t.cookieStoreId||null});
}
async function selectActiveTab(id) {
  const sequence=++selectionSequence;
  let tab;
  try { tab=await api.tabs.get(id); } catch { return; }
  if(sequence!==selectionSequence || !cleanUrl(tab.url))return;
  if(tabId!==tab.id){
    if(scanning)send({type:'scanState',scanId,scanning:false,tabId,message:'Вкладка изменилась. Запустите анализ заново.'});
    scanning=false;resources.clear();requests.clear();segments.clear();pageHeaders={};
  }
  tabId=tab.id;pageUrl=tab.url;
  if(port)await announce().catch(()=>{});
}
api.tabs.onActivated.addListener(info=>{selectActiveTab(info.tabId).catch(()=>{});});
api.windows.onFocusChanged.addListener(id=>{
  if(id===api.windows.WINDOW_ID_NONE)return;
  api.tabs.query({active:true,windowId:id}).then(tabs=>{if(tabs[0])return selectActiveTab(tabs[0].id);}).catch(()=>{});
});
async function inject() {
  if(!scanning||tabId===null)return;
  const id=scanId, selected=tabId;
  try {
    await api.scripting.executeScript({target:{tabId:selected,allFrames:true},func:value=>{globalThis.__streamShelfScanId=value;},args:[id]});
    if(!scanning||scanId!==id||tabId!==selected)return;
    await api.scripting.executeScript({target:{tabId:selected,allFrames:true},files:['content.js']});
  }catch(e){if(scanning&&scanId===id)send({type:'notice',message:'DOM некоторых кадров недоступен: '+e.message});}
}
async function handleCommand(m) {
  if(!m||typeof m.type!=='string')throw new Error('Некорректная команда');
  if(m.type==='helloAck')return;
  if(m.type==='ping'){const hasSiteAccess=await api.permissions.contains({origins:['http://*/*','https://*/*']});const {setupConfirmed}=await api.storage.local.get('setupConfirmed');send({type:'pong',requestId:m.requestId,hasSiteAccess,setupConfirmed:setupConfirmed===true,setupApi:true,setupRevision:'2026-10-31-1'});return;}
  if(m.type==='openSetup') {
    try {
      if(!api.action?.openPopup)throw new Error('Popup API unavailable');
      await api.action.openPopup();
    } catch {
      try { await api.tabs.create({url:api.runtime.getURL('popup.html'),active:true}); }
      catch { send({type:'notice',message:'Откройте StreamShelf Connector через меню расширений браузера.'}); }
    }
    return;
  }
  if(m.type==='scanStart') {
    const id=Number(m.scanId)||0;if(id<scanId)return;scanId=id;
    if(tabId===null||m.tabId!==tabId)throw new Error('Откройте страницу с медиа в браузере и вернитесь в StreamShelf');
    await announce();if(scanId!==id)return;resources.clear();requests.clear();segments.clear();pageHeaders={};scanning=true;
    send({type:'scanState',scanId,scanning:true,tabId});await inject();if(m.reload&&scanning&&scanId===id)await api.tabs.reload(tabId);return;
  }
  if(m.type==='scanStop') {const id=Number(m.scanId)||0;if(id<scanId)return;scanId=id;scanning=false;requests.clear();await api.tabs.sendMessage(tabId,{type:'stopDom',scanId:id}).catch(()=>{});if(scanId===id)send({type:'scanState',scanId,scanning:false,tabId});return;}
  if(m.type==='credentials') {
    if(m.tabId!==tabId||tabId===null)throw new Error('Вкладка изменилась');
    const t=await api.tabs.get(tabId),current=cleanUrl(t.url),target=cleanUrl(m.url);
    if(!target||!(target===current||resources.has(target)))throw new Error('Ресурс не относится к выбранной вкладке');
    const options={url:target};if(t.cookieStoreId)options.storeId=t.cookieStoreId;
    const cookies=await api.cookies.getAll(options);
    // Preserve partition metadata; the application must reject loss-prone Netscape conversion.
    send({type:'credentialsResult',requestId:m.requestId,url:target,cookies:cookies.map(c=>({domain:c.domain,path:c.path,name:c.name,value:c.value,secure:c.secure,httpOnly:c.httpOnly,expirationDate:c.expirationDate??null,partitioned:!!c.partitionKey||!!c.firstPartyDomain})),
      headers:resources.get(target)?.headers||(target===current?pageHeaders:{'User-Agent':navigator.userAgent,'Referer':current})});return;
  }
  throw new Error('Команда не поддерживается');
}
api.runtime.onMessage.addListener((m,sender,reply)=>{
  if(m.type==='status'){reply({connected:!!port,scanning,tabId,lastError,version:VERSION});return false;}
  if(m.type==='connectExtension') {
    (async()=>{const hasSiteAccess=await api.permissions.contains({origins:['http://*/*','https://*/*']});if(!hasSiteAccess)throw new Error('Разрешите доступ к сайтам и повторите подключение');await connect();await api.storage.local.set({setupConfirmed:true});send({type:'setupChanged'});return {ok:true};})()
      .then(reply,e=>reply({error:e.message}));return true;
  }
  if(m.type==='connectTab') {
    (async()=>{const t=await api.tabs.get(m.tabId);if(!cleanUrl(t.url))throw new Error('Выберите обычную HTTP/HTTPS-страницу');const hasSiteAccess=await api.permissions.contains({origins:['http://*/*','https://*/*']});if(!hasSiteAccess)throw new Error('Разрешите доступ к сайтам и повторите подключение');await selectActiveTab(t.id);await connect();await api.storage.local.set({setupConfirmed:true});await announce();send({type:'setupChanged'});return {ok:true};})()
      .then(reply,e=>reply({error:e.message}));return true;
  }
  if(m.type==='dom'&&sender.tab?.id===tabId&&scanning&&m.scanId===scanId) {
    if(m.blobCount)send({type:'notice',message:'Плеер использует blob: адрес. Это локальный объект браузера, а не ссылка на файл. Для поиска исходных запросов включите поиск до воспроизведения или обновите вкладку.'});
    for(const item of (m.items||[]).slice(0,200))found(item.url,item.mime,null,['Resource Timing','Код страницы'].includes(item.source)?item.source:'DOM',false,item.thumbnail||'',String(item.title||'').slice(0,300));reply({ok:true});return false;
  }
});
api.webRequest.onBeforeSendHeaders.addListener(d=>{
  if(!scanning||d.tabId!==tabId)return;
  if(requests.size>2000)requests.clear();
  const headers={};for(const h of d.requestHeaders||[]){const key={'user-agent':'User-Agent','referer':'Referer','authorization':'Authorization','origin':'Origin'}[h.name.toLowerCase()];if(key&&h.value)headers[key]=h.value;}
  requests.set(d.requestId,{url:d.url,headers,scanId});
},{urls:['http://*/*','https://*/*']},['requestHeaders']);
api.webRequest.onHeadersReceived.addListener(d=>{
  if(!scanning||d.tabId!==tabId||requests.get(d.requestId)?.scanId!==scanId)return;
  if(d.statusCode>=400){requests.delete(d.requestId);send({type:'notice',message:`Ресурс недоступен (HTTP ${d.statusCode}). Обновите страницу и повторите анализ.`});return;}
  const headers=Object.fromEntries((d.responseHeaders||[]).map(h=>[h.name.toLowerCase(),h.value]));
  let size=Number(headers['content-length'])||null;
  const range=/\/(\d+)$/.exec(headers['content-range']||'');if(range)size=Number(range[1]);
  if(d.url===pageUrl)pageHeaders=requests.get(d.requestId)?.headers||{};
  found(d.url,headers['content-type']||'',size,'Сеть');
  const item=resources.get(d.url),req=requests.get(d.requestId);
  if(item&&req)item.headers=req.headers;requests.delete(d.requestId);
},{urls:['http://*/*','https://*/*']},['responseHeaders']);
api.webRequest.onErrorOccurred.addListener(d=>requests.delete(d.requestId),{urls:['http://*/*','https://*/*']});
api.tabs.onUpdated.addListener((id,change,tab)=>{
  if(id!==tabId && change.url && tab?.active && cleanUrl(change.url)) {
    api.windows.get(tab.windowId).then(window=>{if(window.focused)return selectActiveTab(id);}).catch(()=>{});
    return;
  }
  if(id!==tabId)return;
  if(change.url){scanning=false;resources.clear();requests.clear();pageUrl=cleanUrl(change.url)||'';send({type:'scanState',scanId,scanning:false,tabId,message:'Адрес вкладки изменился. Запустите поиск заново.'});announce().catch(()=>{});}
  if(change.status==='complete'&&scanning)inject();
});
api.tabs.onRemoved.addListener(id=>{if(id===tabId){tabId=null;scanning=false;resources.clear();requests.clear();send({type:'tabClosed'});}});
// The browser may dismiss the action popup as the permission dialog closes.
// Observe the grant in the service worker as well, so setup does not depend on
// the popup's continuation surviving that transition.
api.permissions.onAdded?.addListener(()=>{
  (async()=>{
    const granted=await api.permissions.contains({origins:['http://*/*','https://*/*']});
    if(!granted)return;
    await api.storage.local.set({setupConfirmed:true});
    await connect(false);
    send({type:'setupChanged'});
  })().catch(()=>{});
});
api.permissions.onRemoved?.addListener(()=>send({type:'setupChanged'}));
api.runtime.onInstalled.addListener(()=>{connect(false).catch(()=>{});});
api.runtime.onStartup.addListener(()=>{connect(false).catch(()=>{});});
api.alarms.create('streamshelf-reconnect',{periodInMinutes:0.5});
api.alarms.onAlarm.addListener(alarm=>{if(alarm.name==='streamshelf-reconnect'&&!port)connect(false).catch(()=>{});});
