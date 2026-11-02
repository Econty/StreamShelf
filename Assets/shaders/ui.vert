uniform vec2 uScreen;
attribute vec2 aPosition;
attribute vec2 aUV;
attribute vec4 aColor;
attribute vec4 aParams;
varying mediump vec2 vUV;
varying mediump vec4 vColor;
varying mediump vec4 vParams;
void main(){vUV=aUV;vColor=aColor;vParams=aParams;gl_Position=vec4(aPosition.x/uScreen.x*2.0-1.0,1.0-aPosition.y/uScreen.y*2.0,0.0,1.0);}
