#version 330 core
layout(location=0) in vec2 aPosition;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
layout(location=3) in vec4 aParams;
uniform vec2 uScreen;
out vec2 vUV;out vec4 vColor;out vec4 vParams;
void main(){gl_Position=vec4(aPosition.x/uScreen.x*2.-1.,1.-aPosition.y/uScreen.y*2.,0.,1.);vUV=aUV;vColor=aColor;vParams=aParams;}
