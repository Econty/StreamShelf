(()=>{
  const api=globalThis.browser||chrome;
  const scanId=globalThis.__streamShelfScanId??0;
  globalThis.__streamShelfScanner?.stop();
  const sent=new Map(),scriptTexts=new WeakMap();let timer,stopped=false,blobReported=false,resourceObserver;
  function post(items,blobCount=0){
    for(let offset=0;offset<items.length;offset+=200)
      api.runtime.sendMessage({type:'dom',scanId,items:items.slice(offset,offset+200)}).catch(()=>{});
    if(blobCount&&!blobReported){blobReported=true;api.runtime.sendMessage({type:'dom',scanId,items:[],blobCount}).catch(()=>{});}
  }
  function candidate(items,value,mime='',source='DOM',thumbnail='',title=''){
    try{
      const u=new URL(value,location.href);
      if(!['http:','https:'].includes(u.protocol)||u.username||u.password||u.href.length>16000)return;
      const previous=sent.get(u.href);
      const current={mime:mime||previous?.mime||'',thumbnail:thumbnail||previous?.thumbnail||'',title:String(title||previous?.title||'').slice(0,300)};
      if(previous && previous.mime===current.mime && previous.thumbnail===current.thumbnail && previous.title===current.title)return;
      if(sent.size>=1000&&!sent.has(u.href)){
        if(mime.startsWith('image/'))return;
        const image=[...sent].find(([,item])=>item.mime.startsWith('image/'));
        if(image)sent.delete(image[0]);else return;
      }
      sent.set(u.href,current);items.push({url:u.href,...current,source});
    }catch{}
  }
  function timing(entries){
    if(stopped)return;const items=[];
    for(const entry of entries){
      // A generic fetch/XHR is not evidence of media content.
      let mime=entry.contentType||'';
      if(!mime&&['video','audio'].includes(entry.initiatorType))mime=entry.initiatorType+'/unknown';
      let url;try{url=new URL(entry.name);}catch{continue;}
      const declared=url.searchParams.get('mime')||url.searchParams.get('type')||'';
      if(!mime&&/^(video|audio)\/[a-z0-9.+-]+$/i.test(declared))mime=declared;
      if(!/^(video|audio|image)\//i.test(mime)&&!/mpegurl|dash\+xml|vtt|ttml/i.test(mime)&&
         !/\.(m3u8|mpd|mp4|webm|m4a|mp3|aac|opus|ogg|wav|flac|vtt|srt)$/i.test(url.pathname))continue;
      candidate(items,url.href,mime,'Resource Timing');
    }
    post(items);
  }
  function scan(){
    if(stopped)return;const items=[];let blobCount=0;
    document.querySelectorAll('video,audio,source,track,a[href],meta[property="og:video"],meta[property="og:video:url"],meta[property="og:video:secure_url"],meta[property="og:audio"],meta[property="og:audio:url"],meta[property="og:image"],img').forEach(e=>{
      const tag=e.tagName;
      const values=[e.currentSrc,e.src,e.href,e.content].filter(Boolean);
      let mime=e.type||'';
      if(!mime){
        const owner=tag==='SOURCE'?e.parentElement?.tagName:tag;
        if(owner==='VIDEO'||owner==='AUDIO')mime=owner.toLowerCase()+'/unknown';
        if(owner==='PICTURE'||tag==='IMG')mime='image/unknown';
        if(tag==='TRACK')mime='text/vtt';
        if(tag==='META'){const property=e.getAttribute('property')||'';mime=property.includes('og:audio')?'audio/unknown':property.includes('og:image')?'image/unknown':'video/unknown';}
      }
      for(const value of values){
        if(String(value).startsWith('blob:')){blobCount++;continue;}
        if(tag==='A'&&!/\.(m3u8|mpd|mp4|webm|m4a|mp3|aac|opus|ogg|wav|flac|vtt|srt)(\?|#|$)/i.test(value))continue;
        candidate(items,value,mime,'DOM',tag==='VIDEO'?e.poster||'':'',e.title||e.getAttribute('aria-label')||'');
      }
      if(tag==='VIDEO'&&e.poster)candidate(items,e.poster,'image/unknown');
      if(['VIDEO','AUDIO','IMG','SOURCE'].includes(tag)){
        for(const attribute of ['data-src','data-original','data-video-src','data-audio-src']){
          const value=e.getAttribute(attribute);if(value)candidate(items,value,mime);
        }
      }
    });
    scanCode(items);
    post(items,blobCount);
    timing(globalThis.performance?.getEntriesByType('resource')||[]);
  }
  function mediaUrl(value){
    try{const u=new URL(value,location.href);return /\.(m3u8|mpd|mp4|webm|m4a|mp3|aac|opus|ogg|wav|flac|mov|mkv|vtt|srt|jpe?g|png|webp|avif)$/i.test(u.pathname)||/^(video|audio)\//i.test(u.searchParams.get('mime')||'');}catch{return false;}
  }
  function scanCode(items){
    let budget=4*1024*1024,nodes=0;
    function walk(value,hint='',depth=0){
      if(depth>16||++nodes>10000||!value||typeof value!=='object')return;
      if(Array.isArray(value)){for(const item of value)walk(item,hint,depth+1);return;}
      let mime=String(value.mimeType||value.mime_type||value.contentType||value.encodingFormat||'');
      if(!/^(audio|video|image)\//i.test(mime)&&!/mpegurl|dash\+xml/i.test(mime))mime='';
      const type=String(value['@type']||value.type||hint).toLowerCase();
      if(!mime){if(/video/.test(type))mime='video/unknown';else if(/audio|voice/.test(type))mime='audio/unknown';else if(/image|photo/.test(type))mime='image/unknown';}
      for(const [key,child] of Object.entries(value)){
        if(typeof child==='string'&&child.length<=16000){
          const field=key.toLowerCase();
          const fieldMime=/thumbnail|poster|image|photo/.test(field)?'image/unknown':/video/.test(field)?'video/unknown':/audio|voice/.test(field)?'audio/unknown':mime;
          if(mediaUrl(child)||(/^(https?:\/\/|\/|\.{1,2}\/)/i.test(child)&&/^(contenturl|url|src|fileurl|file_url|downloadurl|download_url|thumbnailurl|thumbnail|poster|image|video|audio|videourl|audiourl)$/.test(field)&&fieldMime))
            candidate(items,child,fieldMime,'Код страницы',typeof (value.thumbnailUrl||value.poster||value.thumbnail)==='string'?(value.thumbnailUrl||value.poster||value.thumbnail):'',String(value.name||value.title||''));
        }else if(child&&typeof child==='object')walk(child,key,depth+1);
      }
    }
    for(const script of document.querySelectorAll('script')){
      const text=script.textContent||'';
      if(!text||text.length>1024*1024||text.length>budget)continue;budget-=text.length;
      if(scriptTexts.get(script)===text)continue;scriptTexts.set(script,text);
      if(/json/i.test(script.type||'')||/^[\s]*[\[{]/.test(text)){
        try{walk(JSON.parse(text));}catch{}
      }
      // Extract literal media URLs only; never execute or evaluate page code.
      const pattern=/["'](https?:[^"'<>\s]{1,16000})["']/g;
      for(const match of text.matchAll(pattern)){
        let value=match[1];try{value=JSON.parse('"'+value+'"');}catch{value=value.replace(/\\\//g,'/');}
        value=value.replace(/&amp;/g,'&');
        if(mediaUrl(value))candidate(items,value,'','Код страницы');
      }
    }
  }
  function schedule(){clearTimeout(timer);timer=setTimeout(scan,250);}
  const observer=new MutationObserver(schedule);
  observer.observe(document.documentElement,{subtree:true,childList:true,characterData:true,attributes:true,attributeFilter:['src','href','content','poster','title','aria-label','type','data-src','data-original','data-video-src','data-audio-src']});
  const events=['loadedmetadata','loadeddata','play','canplay'];
  for(const name of events)document.addEventListener(name,schedule,true);
  try{resourceObserver=new PerformanceObserver(list=>timing(list.getEntries()));resourceObserver.observe({type:'resource',buffered:true});}catch{}
  globalThis.__streamShelfScanner={stop(){stopped=true;clearTimeout(timer);observer.disconnect();resourceObserver?.disconnect();for(const name of events)document.removeEventListener(name,schedule,true);}};
  if(!globalThis.__streamShelfStopListener){
    api.runtime.onMessage.addListener(m=>{if(m.type==='stopDom' && (m.scanId===undefined || (globalThis.__streamShelfScanId??0)<=m.scanId))globalThis.__streamShelfScanner?.stop();});
    globalThis.__streamShelfStopListener=true;
  }
  scan();
})();
