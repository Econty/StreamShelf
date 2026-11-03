const api=globalThis.browser||chrome;
const status=document.querySelector('#status');
api.runtime.sendMessage({type:'status'}).then(s=>{
  status.textContent=s.connected?`Связь с программой установлена · ${api.runtime.getManifest().version}`:s.lastError||'Программа ещё не подключена';
}).catch(()=>{});
document.querySelector('#connect').onclick=async()=>{
  try {
    const granted=await api.permissions.request({origins:['http://*/*','https://*/*']});
    if(!granted){status.textContent='Доступ к сайтам не предоставлен. Чтобы повторить запрос, нажмите кнопку ещё раз.';return;}
    const r=await api.runtime.sendMessage({type:'connectExtension'});
    status.textContent=r.error||'Проверяем связь с программой. Теперь откройте страницу с медиа в браузере.';
  }catch(e){status.textContent=e.message;}
};
