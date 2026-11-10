const api=globalThis.browser||chrome;
const status=document.querySelector('#status');
const button=document.querySelector('#connect');
const description=document.querySelector('#description');
const privacy=document.querySelector('#privacy');
const origins=['http://*/*','https://*/*'];
const messages={
  en:{description:'Connect the extension to StreamShelf. Your browser will request access to all websites so it can find media on the page and its media servers. Grant access only if you trust the extension. Monitoring starts when requested in StreamShelf and applies to the last active tab.',connect:'Connect extension',privacy:'Cookies are sent to the local application when analyzing or downloading. Built-in browser pages are not accessible.',connected:'Connected to StreamShelf',needsAccess:'Connected to StreamShelf; website access is required.',disconnected:'StreamShelf is not connected yet',denied:'Website access was not granted. Click the button again to retry.',checking:'Checking the connection. Now open a page containing media in your browser.',accessError:'Allow access to websites and try connecting again.'},
  ru:{description:'Подключите расширение к программе. Браузер запросит доступ ко всем сайтам: это позволяет искать ресурсы страницы и медиасерверов. Разрешайте его только если доверяете расширению. Наблюдение начинается по команде в программе и касается последней активной вкладки.',connect:'Подключить расширение',privacy:'Cookies передаются локальному приложению при анализе или скачивании. Встроенные страницы браузера недоступны.',connected:'Связь с программой установлена',needsAccess:'Связь с программой установлена; требуется доступ к сайтам.',disconnected:'Программа ещё не подключена',denied:'Доступ к сайтам не предоставлен. Чтобы повторить запрос, нажмите кнопку ещё раз.',checking:'Проверяем связь с программой. Теперь откройте страницу с медиа в браузере.',accessError:'Разрешите доступ к сайтам и повторите подключение.'}
};
const browserLanguage=()=>/^ru(?:-|$)/i.test(api.i18n?.getUILanguage?.()||globalThis.navigator?.language||'en')?'ru':'en';
let language=browserLanguage(),siteAccess=false,statusKey='disconnected',statusDetail='',statusUntil=0;
function render(){
  const m=messages[language];document.documentElement.lang=language;
  description.textContent=m.description;button.textContent=m.connect;privacy.textContent=m.privacy;
  status.textContent=statusKey ? m[statusKey]+statusDetail : statusDetail;
}
async function updateStatus(){
  try {
    const [s, access]=await Promise.all([api.runtime.sendMessage({type:'status'}),api.permissions.contains({origins})]);
    siteAccess=access;
    language=s.connected && (s.language==='ru'||s.language==='en') ? s.language : browserLanguage();
    if(Date.now()>=statusUntil){
      statusKey=s.connected?(siteAccess?'connected':'needsAccess'):(s.lastError?null:'disconnected');
      statusDetail=s.connected?(siteAccess?' · '+api.runtime.getManifest().version:''):(s.lastError||'');
    }
    render();
  } catch {}
}
render();
button.disabled=true;
api.permissions.contains({origins}).then(value=>{siteAccess=value;}).catch(()=>{}).finally(()=>{button.disabled=false;});
updateStatus();
// Keep an open popup in sync with connection and language changes.
setInterval(updateStatus,1000);
button.onclick=async()=>{
  try {
    // Firefox may display its permission prompt behind the action popup.
    const request=siteAccess?null:api.permissions.request({origins});
    const popup=!!globalThis.browser && !!api.extension?.getViews?.({type:'popup'}).includes(window);
    if(request && popup)window.close();
    const granted=siteAccess || await request;
    if(!granted){statusKey='denied';statusDetail='';statusUntil=Date.now()+8000;render();return;}
    siteAccess=true;
    const r=await api.runtime.sendMessage({type:'connectExtension'});
    statusKey=r.error?null:'checking';statusDetail=r.error||'';statusUntil=Date.now()+8000;render();
  }catch(e){statusKey=null;statusDetail=e?.message||messages[language].accessError;statusUntil=Date.now()+8000;render();}
};
