import http from 'node:http';import fs from 'node:fs';import path from 'node:path';
const allowed=new Map([['/test.png','preview-wide.png'],['/test.jpg','preview-wide.jpg'],['/test.mp4','preview-video.mp4']]);
http.createServer((request,response)=>{
 const name=allowed.get(new URL(request.url,'http://127.0.0.1').pathname);
 if(!name){response.writeHead(404);response.end();return;}
 const file=path.join('tests','fixtures',name),size=fs.statSync(file).size;
 let start=0,end=size-1;const range=/bytes=(\d+)-(\d*)/.exec(request.headers.range||'');
 if(range){start=Number(range[1]);if(range[2])end=Math.min(end,Number(range[2]));}
 if(start>=size||end<start){response.writeHead(416);response.end();return;}
 const headers={'Content-Type':name.endsWith('mp4')?'video/mp4':name.endsWith('jpg')?'image/jpeg':'image/png','Content-Length':end-start+1,'Accept-Ranges':'bytes'};
 if(range)headers['Content-Range']=`bytes ${start}-${end}/${size}`;
 response.writeHead(range?206:200,headers);fs.createReadStream(file,{start,end}).pipe(response);
}).listen(0,'127.0.0.1',function(){console.log('PREVIEW_TEST_PORT='+this.address().port);});
