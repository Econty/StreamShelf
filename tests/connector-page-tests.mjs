import vm from 'node:vm';
import fs from 'node:fs';
import assert from 'node:assert/strict';
const source=fs.readFileSync('extension/chromium-development/popup.js','utf8');
const html=fs.readFileSync('extension/chromium-development/popup.html','utf8');
assert.match(html,/<button id="connect"/);
assert.doesNotMatch(html,/Подключить текущую вкладку|<select/i);
for(const mode of ['ordinary','internal-page','no-http-tabs','denied-then-retry','backend-error']){
  const elements={status:{},connect:{},description:{},privacy:{}};
  const messages=[];let allowed=mode!=='denied-then-retry';let requests=0;
  // No tab APIs are provided: setup must work on an internal page or with no HTTP tabs.
  const api={i18n:{getUILanguage:()=> 'ru'},permissions:{contains:async()=>false,request:async()=>{requests++;return allowed;}},runtime:{getManifest:()=>({version:'1.0.3'}),sendMessage:async m=>{messages.push(m);return m.type==='status'?{connected:false}:mode==='backend-error'?{error:'Native connection failed'}:{ok:true};}}};
  const context=vm.createContext({chrome:api,document:{querySelector:s=>elements[s.slice(1)],documentElement:{}},setInterval:()=>0,console});
  vm.runInContext(source,context);
  await new Promise(resolve=>setTimeout(resolve,0));
  await elements.connect.onclick();
  if(!allowed){
    assert.equal(messages.filter(m=>m.type==='connectExtension').length,0);
    assert.match(elements.status.textContent,/повторить запрос/);
    allowed=true;await elements.connect.onclick();assert.equal(requests,2);
  }
  assert.equal(messages.filter(m=>m.type==='connectExtension').length,1);
  if(mode==='backend-error') assert.equal(elements.status.textContent,'Native connection failed');
  else assert.match(elements.status.textContent,/Проверяем связь/);
  console.log(`PASS connector interface: ${mode}`);
}
