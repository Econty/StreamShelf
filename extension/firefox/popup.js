const api=globalThis.browser||chrome;
const status=document.querySelector('#status');
const button=document.querySelector('#connect');
const origins=['http://*/*','https://*/*'];
let siteAccess=false;
button.disabled=true;
api.permissions.contains({origins}).then(value=>{siteAccess=value;}).catch(()=>{}).finally(()=>{button.disabled=false;});
api.runtime.sendMessage({type:'status'}).then(s=>{
  status.textContent=s.connected?`Связь с программой установлена · ${api.runtime.getManifest().version}`:s.lastError||'Программа ещё не подключена';
}).catch(()=>{});
button.onclick=async()=>{
  try {
    // Firefox may display the permission prompt behind the action popup.
    // Request while the click is active, then close only the action popup.
    const request=siteAccess ? null : api.permissions.request({origins});
    const popup=!!api.extension?.getViews?.({type:'popup'}).includes(window);
    if(request && popup) window.close();
    const granted=siteAccess || await request;
    if(!granted){status.textContent='Доступ к сайтам не предоставлен. Чтобы повторить запрос, нажмите кнопку ещё раз.';return;}
    const r=await api.runtime.sendMessage({type:'connectExtension'});
    status.textContent=r.error||'Проверяем связь с программой. Теперь откройте страницу с медиа в браузере.';
  }catch(e){status.textContent=e.message;}
};
