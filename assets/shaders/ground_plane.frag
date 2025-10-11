#version 330 core
in vec3 v_worldPos;
in vec3 v_normal;
in vec2 v_uv;
layout (location = 0) out vec4 FragColor;
uniform vec3 u_grassPrimary;
uniform vec3 u_grassSecondary;
uniform vec3 u_grassDry;
uniform vec3 u_soilColor;
uniform vec3 u_tint;
uniform vec2 u_noiseOffset;
uniform float u_tileSize;
uniform float u_macroNoiseScale;
uniform float u_detailNoiseScale;
uniform float u_soilBlendHeight;
uniform float u_soilBlendSharpness;
uniform float u_ambientBoost;
uniform vec3 u_lightDir;

float hash21(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453123);}
float noise21(vec2 p){vec2 i=floor(p),f=fract(p);float a=hash21(i),b=hash21(i+vec2(1,0)),c=hash21(i+vec2(0,1)),d=hash21(i+vec2(1,1));vec2 u=f*f*(3.0-2.0*f);return mix(mix(a,b,u.x),mix(c,d,u.x),u.y);}
float fbm(vec2 p){float v=0.0,a=0.5;for(int i=0;i<3;++i){v+=noise21(p)*a;p=p*2.07+13.17;a*=0.5;}return v;}

void main(){
    vec3 n=normalize(v_normal);
    float ts=max(u_tileSize,1e-4);
    vec2 wuv=(v_worldPos.xz/ts)+u_noiseOffset;
    float macro=fbm(wuv*u_macroNoiseScale);
    float detail=noise21(wuv*(u_detailNoiseScale*2.0));
    float patchNoise=fbm(wuv*u_macroNoiseScale*0.4);
    float moistureVar=smoothstep(0.3,0.7,patchNoise);
    float lush=smoothstep(0.2,0.8,macro);
    lush=mix(lush,moistureVar,0.3);
    vec3 lushGrass=mix(u_grassPrimary,u_grassSecondary,lush);
    float dryness=clamp(0.3*detail,0.0,0.4);
    dryness+=moistureVar*0.15;
    vec3 grassCol=mix(lushGrass,u_grassDry,dryness);
    float sw=max(0.01,1.0/max(u_soilBlendSharpness,1e-3));
    float sN=(noise21(wuv*4.0+9.7)-0.5)*sw*0.8;
    float soilMix=1.0-smoothstep(u_soilBlendHeight-sw+sN,u_soilBlendHeight+sw+sN,v_worldPos.y);
    soilMix=clamp(soilMix,0.0,1.0);
    float mudPatch=fbm(wuv*0.08+vec2(7.3,11.2));
    mudPatch=smoothstep(0.65,0.75,mudPatch);
    soilMix=max(soilMix,mudPatch*0.85);
    vec3 baseCol=mix(grassCol,u_soilColor,soilMix);
    vec3 dx=dFdx(v_worldPos),dy=dFdy(v_worldPos);
    float mScale=u_detailNoiseScale*8.0/ts;
    float h0=noise21(wuv*mScale);
    float hx=noise21((wuv+dx.xz*mScale));
    float hy=noise21((wuv+dy.xz*mScale));
    vec2 g=vec2(hx-h0,hy-h0);
    vec3 t=normalize(dx - n*dot(n,dx));
    vec3 b=normalize(cross(n,t));
    float microAmp=0.12;
    vec3 nMicro=normalize(n-(t*g.x + b*g.y)*microAmp);
    float jitter=(hash21(wuv*0.27+vec2(17.0,9.0))-0.5)*0.06;
    float brightnessVar=(moistureVar-0.5)*0.08;
    vec3 col=baseCol*(1.0+jitter+brightnessVar);
    col*=u_tint;
    vec3 L=normalize(u_lightDir);
    float ndl=max(dot(nMicro,L),0.0);
    float ambient=0.40;
    float fres=pow(1.0-max(dot(nMicro,vec3(0,1,0)),0.0),2.0);
    float roughnessVar=mix(0.65,0.95,1.0-moistureVar);
    float specContrib=fres*0.08*roughnessVar;
    float shade=ambient+ndl*0.65+specContrib;
    vec3 lit=col*shade*u_ambientBoost;
    FragColor=vec4(clamp(lit,0.0,1.0),1.0);
}
