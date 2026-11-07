#pragma once
// Original procedural ambient music + tiny synthesized sfx (no copyrighted audio).
#include "raylib.h"
#include <cmath>
#include <cstdlib>

struct Voice{float f=0,ph=0,amp=0;};
struct Synth{
  AudioStream st{}; bool ok=false; Voice v[4]; int next=0; int vi=0;
  void init(){
    if(!IsAudioDeviceReady())return;
    SetAudioStreamBufferSizeDefault(4096);
    st=LoadAudioStream(44100,16,1); SetAudioStreamVolume(st,0.5f); PlayAudioStream(st); ok=true;
  }
  void update(bool on){
    if(!ok||!IsAudioStreamProcessed(st))return;
    static short buf[4096];
    static const float sc[7]={261.63f,293.66f,329.63f,392.00f,440.00f,523.25f,587.33f};
    for(int i=0;i<4096;i++){
      if(--next<=0){
        if(on){v[vi].f=sc[rand()%7]*((rand()%3==0)?0.5f:1.0f);v[vi].ph=0;v[vi].amp=0.35f;vi=(vi+1)&3;}
        next=44100*(1+rand()%3)+rand()%30000;
      }
      float s=0;
      for(auto&x:v){
        if(x.amp<0.0005f)continue;
        s+=(sinf(x.ph)+0.35f*sinf(2*x.ph)+0.15f*sinf(3*x.ph))*x.amp;
        x.ph+=6.2831853f*x.f/44100.f; if(x.ph>6.2831853f)x.ph-=6.2831853f;
        x.amp*=0.99997f;
      }
      buf[i]=(short)(s*9000);
    }
    UpdateAudioStream(st,buf,4096);
  }
};
inline Sound makeSfx(float f0,float f1,float dur,float noise){
  int n=(int)(22050*dur); short*d=(short*)MemAlloc(n*sizeof(short)); float ph=0;
  for(int i=0;i<n;i++){
    float t=(float)i/n,f=f0+(f1-f0)*t; ph+=6.2831853f*f/22050.f;
    float s=sinf(ph)*(1-noise)+(((rand()%2000)/1000.f)-1.f)*noise;
    d[i]=(short)(s*(1-t)*9000);
  }
  Wave w{}; w.frameCount=n;w.sampleRate=22050;w.sampleSize=16;w.channels=1;w.data=d;
  Sound s=LoadSoundFromWave(w); MemFree(d); return s;
}
