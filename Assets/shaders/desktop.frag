#version 330 core
// Desktop adaptation of the supplied KRECHETOV-LABS ui.frag: analytic rounded geometry + SDF atlas.
uniform sampler2D uFont;
uniform sampler2D uSnapshot;
uniform sampler2D uImage;
in vec2 vUV;in vec4 vColor;in vec4 vParams;out vec4 outputColor;
void main(){
 if(vParams.w>6.5){outputColor=texture(uImage,vUV)*vColor;return;}
 if(vParams.w>5.5){vec2 stepUV=vec2(5.)/vec2(textureSize(uSnapshot,0));vec4 sum=vec4(0.);float total=0.;for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++){float weight=float((x==0?2:1)*(y==0?2:1));sum+=texture(uSnapshot,vUV+vec2(x,y)*stepUV)*weight;total+=weight;}outputColor=sum/total*vColor;return;}
 if(vParams.w>4.5){outputColor=texture(uSnapshot,vUV)*vColor;return;}
 float coverage=1.;
 if(vParams.w>3.5){float d=length((vUV-.5)*2.);coverage=exp(-d*d*3.)*(1.-smoothstep(.75,1.,d));}
 else if(vParams.w>2.5){float sdf=texture(uFont,vUV).r;float aa=max(fwidth(sdf)*.8,.008);coverage=smoothstep(.5-aa,.5+aa,sdf);}
 else if(vParams.w>.5){vec2 p=(vUV-.5)*vParams.xy;float d;
  if(vParams.w>1.5)d=length(p)-vParams.z;
  else{vec2 q=abs(p)-vParams.xy*.5+vParams.z;d=length(max(q,vec2(0.)))+min(max(q.x,q.y),0.)-vParams.z;}
  float aa=max(fwidth(d),.7);coverage=1.-smoothstep(-aa,aa,d);
 }
 outputColor=vec4(vColor.rgb,vColor.a*coverage);
}
