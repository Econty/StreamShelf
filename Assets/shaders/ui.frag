precision mediump float;
uniform sampler2D uFont;
uniform sampler2D uIcons;
varying mediump vec2 vUV;
varying mediump vec4 vColor;
varying mediump vec4 vParams;
void main(){
 if(vParams.w>4.5){gl_FragColor=texture2D(uIcons,vUV)*vColor;return;}
 float coverage=1.0;
 if(vParams.w>3.5){coverage=smoothstep(0.48,1.2,length((vUV-0.5)*2.0));}
 else if(vParams.w>2.5){float sdf=texture2D(uFont,vUV).r;coverage=smoothstep(vParams.y-vParams.x,vParams.y+vParams.x,sdf);}
 else if(vParams.w>0.5){vec2 p=(vUV-0.5)*vParams.xy;float d;
  if(vParams.w>1.5)d=length(p)-vParams.z;
  else{vec2 q=abs(p)-vParams.xy*0.5+vParams.z;d=length(max(q,vec2(0.0)))+min(max(q.x,q.y),0.0)-vParams.z;}
  coverage=1.0-smoothstep(-0.7,0.7,d);
 }
 gl_FragColor=vec4(vColor.rgb,vColor.a*coverage);
}
