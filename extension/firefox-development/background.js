'use strict';
const api = globalThis.browser || chrome;
const VERSION = api.runtime.getManifest().version;
let port = null, tabId = null, scanning = false, pageUrl = '', instanceId = '', lastError = '';
let appLanguage = null;
function tr(ru,en) { const locale=appLanguage || api.i18n?.getUILanguage?.() || globalThis.navigator?.language || 'en'; return /^ru(?:-|$)/i.test(locale) ? ru : en; }
let connecting = null, reconnectDelay = 1000, reconnectTimer = null;
function scheduleReconnect(){
  if(reconnectTimer!==null)return;
  const delay=reconnectDelay;reconnectDelay=Math.min(reconnectDelay*2,30000);
  // Alarms survive service-worker suspension; the timer makes an active worker retry promptly.
  api.alarms.create('streamshelf-reconnect',{delayInMinutes:Math.max(0.5,delay/60000)});
  reconnectTimer=setTimeout(()=>{reconnectTimer=null;if(!port)connect(false).catch(()=>scheduleReconnect());},delay);
}
let selectionSequence = 0, scanId = 0, sourceId = 0, baselineSource = -1, cookieBaseline = null, activeCookieStore = null;
const resources = new Map(), requests = new Map(), segments = new Map();let pageHeaders={};
function send(message) { if(port) { try { port.postMessage(message); } catch { lastError=tr('Связь с приложением потеряна','Connection to StreamShelf was lost'); } } }
function cleanUrl(value) { try { const u=new URL(value); return ['http:','https:'].includes(u.protocol)&&!u.username&&!u.password?u.href:null; } catch {return null;} }
function classify(url,mime='') {
  let path,u;try {u=new URL(url);path=u.pathname.toLowerCase();}catch{return null;}
  mime=String(mime).toLowerCase().split(';')[0].trim();
  if(/^(text\/html|text\/css|(?:application|text)\/(?:json|javascript)|font\/)/.test(mime))return null;
  // Generic XML is commonly used for DASH. Keep rejecting other XML pages.
  if(/^(?:application|text)\/xml(?:$|\+)/.test(mime)&&!path.endsWith('.mpd'))return null;
  if(!mime||mime==='application/octet-stream'){
    const declared=u.searchParams.get('mime')||u.searchParams.get('type')||'';
    if(/^(video|audio)\/[a-z0-9.+-]+$/i.test(declared)||/^(application\/(vnd\.apple\.mpegurl|x-mpegurl|dash\+xml))$/i.test(declared))mime=declared.toLowerCase();
  }
  if(/(?:^|\/)(?:widevine|playready|licen[cs]e)(?:\/|$)/.test(path)&&!mime.startsWith('image/'))return 'DRM / лицензия · кандидат';
  if(path.endsWith('.m3u8')||mime.includes('mpegurl'))return 'HLS';
  if(path.endsWith('.mpd')||mime.includes('dash+xml'))return 'DASH';
  if(/\.(m4s|ts|cmfv|cmfa)$/.test(path))return 'Сегмент';
  if(mime.startsWith('video/')||/\.(mp4|webm|mkv|mov|ogv)$/.test(path))return 'Видео';
  if(mime.startsWith('audio/')||/\.(m4a|mp3|aac|opus|ogg|flac|wav)$/.test(path))return 'Аудио';
  if(mime.includes('vtt')||mime.includes('ttml')||/\.(vtt|srt|ttml)$/.test(path))return 'Субтитры';
  if(mime.startsWith('image/')||/\.(jpe?g|png|webp|avif|gif|svg)$/.test(path))return 'Изображения';
  return null;
}
function serializeCookies(cookies) {
  return cookies.map(c=>({domain:c.domain,path:c.path,name:c.name,value:c.value,secure:c.secure,httpOnly:c.httpOnly,expirationDate:c.expirationDate??null,partitioned:!!c.partitionKey||!!c.firstPartyDomain}));
}
async function cookieStoreForTab(tab) {
  if(tab.cookieStoreId)return tab.cookieStoreId;
  if(api.cookies.getAllCookieStores) {
    const stores=await api.cookies.getAllCookieStores();
    const store=stores.find(entry=>entry.tabIds?.includes(tab.id));
    if(!store)throw new Error(tr('Не удалось определить хранилище cookies вкладки','Could not identify the tab cookie store'));
    return store.id;
  } else if(tab.incognito) throw new Error(tr('Не удалось определить хранилище cookies приватной вкладки','Could not identify the private tab cookie store'));
  return null;
}
async function cookiesForTab(target, tab, knownStore) {
  const options={url:target};
  const store=knownStore===undefined?await cookieStoreForTab(tab):knownStore;
  if(store)options.storeId=store;
  return api.cookies.getAll(options);
}
function cookieFingerprint(cookies) {
  return JSON.stringify(cookies.map(c=>JSON.stringify([c.domain,c.path,c.name,c.value,!!c.secure,!!c.httpOnly,c.partitionKey||null,c.firstPartyDomain||null])).sort());
}
function relevantCookie(cookie) {
  if(activeCookieStore && cookie?.storeId && activeCookieStore!==cookie.storeId)return false;
  const domain=String(cookie?.domain||'').replace(/^\./,'').toLowerCase();
  if(!domain)return true;
  // Partition keys identify the top-level site, not the resource host. If the
  // key cannot be interpreted, stay conservative and invalidate the snapshot.
  const firstParty=cookie?.partitionKey?.topLevelSite||cookie?.firstPartyDomain;
  if(firstParty&&pageUrl)try{
    const top=new URL(pageUrl).hostname.toLowerCase();
    const site=(String(firstParty).includes('://')?new URL(firstParty).hostname:String(firstParty)).replace(/^\./,'').toLowerCase();
    if(site&&top!==site&&!top.endsWith('.'+site))return false;
  }catch{}
  const path=typeof cookie?.path==='string'&&cookie.path.startsWith('/')?cookie.path:'/';
  return [pageUrl,...resources.keys()].some(url=>{try{
    const parsed=new URL(url),host=parsed.hostname.toLowerCase(),requestPath=parsed.pathname||'/';
    const domainMatches=host===domain||(!cookie.hostOnly&&host.endsWith('.'+domain));
    const pathMatches=requestPath===path||(requestPath.startsWith(path)&&(path.endsWith('/')||requestPath[path.length]==='/'));
    return domainMatches&&pathMatches&&(!cookie.secure||parsed.protocol==='https:');
  }catch{return false;}});
}
function invalidateCookieSource() {
  if(tabId===null)return;
  ++sourceId;baselineSource=-1;cookieBaseline=null;
  if(scanning)api.tabs.sendMessage(tabId,{type:'stopDom',scanId}).catch(()=>{});
  scanning=false;resources.clear();requests.clear();segments.clear();pageHeaders={};
  send({type:'scanState',scanId,scanning:false,tabId,message:tr('Доступ к сайту изменился. Запустите анализ заново.','Site credentials changed. Analyze the page again.')});
  announce().catch(()=>{});
}
api.cookies?.onChanged?.addListener(change=>{if(relevantCookie(change?.cookie))invalidateCookieSource();});
function found(url,mime='',size=null,source='Сеть',drm=false,thumbnail='',title='',requestHeaders=null) {
  url=cleanUrl(url);if(!url||url.length>16000)return;
  const kind=classify(url,mime);if(!kind)return;if(kind==='Сегмент'){const u=new URL(url);const group=u.origin+u.pathname.slice(0,u.pathname.lastIndexOf('/')+1);const n=(segments.get(group)||0)+1;segments.set(group,n);if(n===1||n%20===0)send({type:'notice',message:tr(`Обнаружены сегменты потока: ${n}. Ищите исходный HLS/DASH-манифест.`,`Detected ${n} stream segments. Look for the original HLS/DASH manifest.`)});return;}
  if(resources.size>=1000&&!resources.has(url)){
    if(kind==='Изображения')return;
    const image=[...resources].find(([,item])=>item.kind==='Изображения');
    if(image)resources.delete(image[0]);else return;
  }
  const old=resources.get(url);const item={url,mime:mime||old?.mime||'',kind,size:size??old?.size??null,source,drm:drm||old?.drm||kind.startsWith('DRM'),thumbnail:cleanUrl(thumbnail)||old?.thumbnail||'',title:title||old?.title||'',time:Date.now()};
  resources.set(url,{...old,...item,...(requestHeaders?{headers:requestHeaders}:{})});
  if(!old||old.mime!==item.mime||old.size!==item.size||old.thumbnail!==item.thumbnail||old.title!==item.title)send({type:'resource',scanId,tabId,pageUrl,sourceId,item});
}
async function connect(allowLaunch=true) {
  if(port)return;
  if(connecting)return connecting;
  connecting=(async()=>{
  const stored=await api.storage.local.get('instanceId');instanceId=stored.instanceId||crypto.randomUUID();await api.storage.local.set({instanceId});
  const opened=api.runtime.connectNative('dev.streamshelf.connector');port=opened;
  opened.onDisconnect.addListener(()=>{if(port!==opened)return;if(scanning&&tabId!==null)api.tabs.sendMessage(tabId,{type:'stopDom',scanId}).catch(()=>{});lastError=opened.error?.message||api.runtime.lastError?.message||tr('Приложение отключено','StreamShelf disconnected');port=null;appLanguage=null;scanning=false;requests.clear();resources.clear();scheduleReconnect();});
  opened.onMessage.addListener(message=>{handleCommand(message).catch(e=>send({type:'error',requestId:message.requestId,message:e.message}));});
  send({type:'hello',protocol:1,version:VERSION,instanceId,browser:navigator.userAgent,allowLaunch,capabilities:['network','dom','cookies','tabReload','language','scopedCredentials']});
  if(tabId===null){const tabs=await api.tabs.query({active:true,lastFocusedWindow:true}).catch(()=>[]);if(tabs[0])await selectActiveTab(tabs[0].id);}
  if(tabId!==null)await announce();
  })();
  try { await connecting; } finally { connecting=null; }
}
async function announce() {
  if(tabId===null)return;
  const selected=tabId, sequence=selectionSequence, source=sourceId;
  const t=await api.tabs.get(selected);
  if(tabId!==selected || selectionSequence!==sequence || sourceId!==source || !t || t.id!==selected)return;
  pageUrl=cleanUrl(t.url)||'';
  if((baselineSource!==source || cookieBaseline===null) && pageUrl) {
    let snapshot=null,store=null;
    try {store=await cookieStoreForTab(t);snapshot=cookieFingerprint(await cookiesForTab(pageUrl,t,store));} catch {}
    if(tabId!==selected || selectionSequence!==sequence || sourceId!==source)return;
    baselineSource=source;cookieBaseline=snapshot;activeCookieStore=store;
  }
  send({type:'tabSelected',tabId:selected,url:pageUrl,title:t.title||'',storeId:t.cookieStoreId||null,sourceId:source});
}
async function selectActiveTab(id) {
  const sequence=++selectionSequence;
  let tab;
  try { tab=await api.tabs.get(id); } catch { return; }
  if(sequence!==selectionSequence)return;
  if(scanning && (tabId!==tab.id || !cleanUrl(tab.url)) && tabId!==null)
    api.tabs.sendMessage(tabId,{type:'stopDom',scanId}).catch(()=>{});
  if(!cleanUrl(tab.url)) {
    if(scanning)send({type:'scanState',scanId,scanning:false,tabId,message:tr('Вкладка изменилась. Запустите анализ заново.','The tab changed. Start the analysis again.')});
    tabId=null;pageUrl='';activeCookieStore=null;scanning=false;resources.clear();requests.clear();segments.clear();pageHeaders={};
    send({type:'tabClosed'});return;
  }
  if(tabId!==tab.id){
    ++sourceId;activeCookieStore=null;
    if(scanning)send({type:'scanState',scanId,scanning:false,tabId,message:tr('Вкладка изменилась. Запустите анализ заново.','The tab changed. Start the analysis again.')});
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
  }catch(e){if(scanning&&scanId===id)send({type:'notice',message:tr('DOM некоторых кадров недоступен: ','DOM of some frames is unavailable: ')+e.message});}
}
async function handleCommand(m) {
  if(!m||typeof m.type!=='string')throw new Error(tr('Некорректная команда','Invalid command'));
  if(m.type==='language'){appLanguage=m.language==='ru'||m.language==='en'?m.language:null;return;}
  if(m.type==='helloAck'){reconnectDelay=1000;if(reconnectTimer!==null){clearTimeout(reconnectTimer);reconnectTimer=null;}return;}
  if(m.type==='ping'){const hasSiteAccess=await api.permissions.contains({origins:['http://*/*','https://*/*']});const {setupConfirmed}=await api.storage.local.get('setupConfirmed');send({type:'pong',requestId:m.requestId,hasSiteAccess,setupConfirmed:setupConfirmed===true,setupApi:true,setupRevision:'2026-10-31-1'});return;}
  if(m.type==='openSetup') {
    try {
      if(!api.action?.openPopup)throw new Error('Popup API unavailable');
      await api.action.openPopup();
    } catch {
      try { await api.tabs.create({url:api.runtime.getURL('popup.html'),active:true}); }
      catch { send({type:'notice',message:tr('Откройте StreamShelf Connector через меню расширений браузера.','Open StreamShelf Connector from the browser extensions menu.')}); }
    }
    return;
  }
  if(m.type==='scanStart') {
    const id=Number(m.scanId)||0;if(id<scanId)return;scanId=id;
    if(tabId===null||m.tabId!==tabId)throw new Error(tr('Откройте страницу с медиа в браузере и вернитесь в StreamShelf','Open a media page in your browser and return to StreamShelf'));
    const selected=tabId, sequence=selectionSequence, source=sourceId;
    await announce();if(scanId!==id || tabId!==selected || selectionSequence!==sequence || sourceId!==source)return;
    resources.clear();requests.clear();segments.clear();pageHeaders={};scanning=true;
    send({type:'scanState',scanId,scanning:true,tabId:selected});await inject();
    if(m.reload&&scanning&&scanId===id&&tabId===selected&&sourceId===source)await api.tabs.reload(selected);return;
  }
  if(m.type==='scanStop') {const id=Number(m.scanId)||0;if(id<scanId)return;scanId=id;scanning=false;requests.clear();if(tabId!==null)await api.tabs.sendMessage(tabId,{type:'stopDom',scanId:id}).catch(()=>{});if(scanId===id)send({type:'scanState',scanId,scanning:false,tabId});return;}
  if(m.type==='credentials') {
    if(m.tabId!==tabId||tabId===null)throw new Error(tr('Вкладка изменилась','The tab changed'));
    const selected=tabId, sequence=selectionSequence, source=sourceId, sourcePage=pageUrl;
    const unchanged=()=>tabId===selected && selectionSequence===sequence && sourceId===source && (m.sourceId===undefined || m.sourceId===source);
    if(!unchanged())throw new Error(tr('Источник страницы изменился','The page source changed'));
    const t=await api.tabs.get(selected);
    if(!unchanged() || !t || t.id!==selected || (m.storeId && t.cookieStoreId!==m.storeId))throw new Error(tr('Вкладка изменилась','The tab changed'));
    const current=cleanUrl(t.url),target=cleanUrl(m.url);
    if(!current || current!==sourcePage)throw new Error(tr('Адрес вкладки изменился','The tab URL changed'));
    if(!target||!(target===current||resources.has(target)))throw new Error(tr('Ресурс не относится к выбранной вкладке','The resource does not belong to the selected tab'));
    const headers=resources.get(target)?.headers||(target===current?pageHeaders:{'User-Agent':navigator.userAgent,'Referer':current});
    if(baselineSource!==source || cookieBaseline===null)throw new Error(tr('Снимок доступа недоступен. Повторите анализ.','The credential snapshot is unavailable. Analyze the page again.'));
    const pageCookies=await cookiesForTab(sourcePage,t);
    if(!unchanged())throw new Error(tr('Вкладка изменилась','The tab changed'));
    if(cookieFingerprint(pageCookies)!==cookieBaseline){invalidateCookieSource();throw new Error(tr('Доступ к сайту изменился','Site credentials changed'));}
    const cookies=target===sourcePage?pageCookies:await cookiesForTab(target,t);
    const confirmed=await cookiesForTab(sourcePage,t);
    if(!unchanged())throw new Error(tr('Вкладка изменилась','The tab changed'));
    if(cookieFingerprint(confirmed)!==cookieBaseline){invalidateCookieSource();throw new Error(tr('Доступ к сайту изменился','Site credentials changed'));}
    const after=await api.tabs.get(selected);
    if(!unchanged() || !after || after.id!==selected || cleanUrl(after.url)!==sourcePage || (t.cookieStoreId||null)!==(after.cookieStoreId||null))throw new Error(tr('Вкладка изменилась','The tab changed'));
    // Preserve partition metadata; the application must reject loss-prone Netscape conversion.
    send({type:'credentialsResult',requestId:m.requestId,url:m.url,tabId:selected,sourceId:source,storeId:t.cookieStoreId||null,cookies:serializeCookies(cookies),headers});return;
  }
  throw new Error(tr('Команда не поддерживается','Unsupported command'));
}
api.runtime.onMessage.addListener((m,sender,reply)=>{
  if(m.type==='status'){reply({connected:!!port,scanning,tabId,lastError,version:VERSION,language:appLanguage});return false;}
  if(m.type==='connectExtension') {
    (async()=>{const hasSiteAccess=await api.permissions.contains({origins:['http://*/*','https://*/*']});if(!hasSiteAccess)throw new Error(tr('Разрешите доступ к сайтам и повторите подключение','Allow website access and retry the connection'));await connect();await api.storage.local.set({setupConfirmed:true});send({type:'setupChanged'});return {ok:true};})()
      .then(reply,e=>reply({error:e.message}));return true;
  }
  if(m.type==='connectTab') {
    (async()=>{const t=await api.tabs.get(m.tabId);if(!cleanUrl(t.url))throw new Error(tr('Выберите обычную HTTP/HTTPS-страницу','Choose a regular HTTP/HTTPS page'));const hasSiteAccess=await api.permissions.contains({origins:['http://*/*','https://*/*']});if(!hasSiteAccess)throw new Error(tr('Разрешите доступ к сайтам и повторите подключение','Allow website access and retry the connection'));await selectActiveTab(t.id);await connect();await api.storage.local.set({setupConfirmed:true});await announce();send({type:'setupChanged'});return {ok:true};})()
      .then(reply,e=>reply({error:e.message}));return true;
  }
  if(m.type==='dom'&&sender.tab?.id===tabId&&scanning&&m.scanId===scanId) {
    if(m.blobCount)send({type:'notice',message:tr('Плеер использует blob: адрес. Это локальный объект браузера, а не ссылка на файл. Для поиска исходных запросов включите поиск до воспроизведения или обновите вкладку.','The player uses a blob: URL. It is a local browser object, not a file link. Start searching before playback or reload the tab to find the original requests.')});
    for(const item of (m.items||[]).slice(0,200))found(item.url,item.mime,null,['Resource Timing','Код страницы'].includes(item.source)?item.source:'DOM',false,item.thumbnail||'',String(item.title||'').slice(0,300));reply({ok:true});return false;
  }
});
const captureRequestHeaders=d=>{
  if(!scanning||d.tabId!==tabId)return;
  if(requests.size>2000)requests.clear();
  const headers={};for(const h of d.requestHeaders||[]){const key={'user-agent':'User-Agent','referer':'Referer','authorization':'Authorization','origin':'Origin'}[h.name.toLowerCase()];if(key&&h.value)headers[key]=h.value;}
  const previous=resources.get(cleanUrl(d.url))?.headers;
  if(previous && (previous.Authorization||headers.Authorization) && previous.Authorization!==headers.Authorization){invalidateCookieSource();return;}
  requests.set(d.requestId,{url:d.url,headers,scanId});
};
try { api.webRequest.onBeforeSendHeaders.addListener(captureRequestHeaders,{urls:['http://*/*','https://*/*']},['requestHeaders','extraHeaders']); }
catch { api.webRequest.onBeforeSendHeaders.addListener(captureRequestHeaders,{urls:['http://*/*','https://*/*']},['requestHeaders']); }
api.webRequest.onHeadersReceived.addListener(d=>{
  if(!scanning||d.tabId!==tabId||requests.get(d.requestId)?.scanId!==scanId)return;
  if(d.statusCode>=400){requests.delete(d.requestId);if(classify(d.url))send({type:'notice',message:tr(`Ресурс недоступен (HTTP ${d.statusCode}). Обновите страницу и повторите анализ.`,`Resource unavailable (HTTP ${d.statusCode}). Reload the page and retry analysis.`)});return;}
  const headers=Object.fromEntries((d.responseHeaders||[]).map(h=>[h.name.toLowerCase(),h.value]));
  let size=Number(headers['content-length'])||null;
  const range=/\/(\d+)$/.exec(headers['content-range']||'');if(range)size=Number(range[1]);
  if(d.url===pageUrl)pageHeaders=requests.get(d.requestId)?.headers||{};
  const req=requests.get(d.requestId);
  found(d.url,headers['content-type']||'',size,'Сеть',false,'','',req?.headers);
  requests.delete(d.requestId);
},{urls:['http://*/*','https://*/*']},['responseHeaders']);
api.webRequest.onErrorOccurred.addListener(d=>requests.delete(d.requestId),{urls:['http://*/*','https://*/*']});
api.tabs.onUpdated.addListener((id,change,tab)=>{
  if(id!==tabId && change.url && tab?.active) {
    api.windows.get(tab.windowId).then(window=>{if(window.focused)return selectActiveTab(id);}).catch(()=>{});
    return;
  }
  if(id!==tabId)return;
  if(change.url && !cleanUrl(change.url)){selectActiveTab(id).catch(()=>{});return;}
  if(change.url && pageUrl && cleanUrl(change.url) && cleanUrl(change.url).split('#')[0]===pageUrl.split('#')[0]){if(cleanUrl(change.url)!==pageUrl)++sourceId;pageUrl=cleanUrl(change.url);announce().catch(()=>{});return;}
  if(change.url){++sourceId;if(scanning)api.tabs.sendMessage(id,{type:'stopDom',scanId}).catch(()=>{});scanning=false;resources.clear();requests.clear();segments.clear();pageHeaders={};pageUrl=cleanUrl(change.url)||'';send({type:'scanState',scanId,scanning:false,tabId,message:tr('Адрес вкладки изменился. Запустите поиск заново.','The tab URL changed. Start the search again.')});announce().catch(()=>{});}
  if(change.status==='loading'&&!change.url){++sourceId;announce().catch(()=>{});}
  if(change.status==='complete'&&scanning)inject();
});
api.tabs.onRemoved.addListener(id=>{if(id===tabId){++selectionSequence;++sourceId;tabId=null;activeCookieStore=null;scanning=false;resources.clear();requests.clear();send({type:'tabClosed'});}});
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
api.runtime.onInstalled.addListener(()=>{connect(false).catch(()=>scheduleReconnect());});
api.runtime.onStartup.addListener(()=>{connect(false).catch(()=>scheduleReconnect());});
api.alarms.onAlarm.addListener(alarm=>{if(alarm.name==='streamshelf-reconnect'&&!port)connect(false).catch(()=>scheduleReconnect());});

// Reconnect when Chrome starts this worker after the extension was re-enabled.
connect(false).catch(()=>scheduleReconnect());
