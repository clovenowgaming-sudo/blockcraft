#pragma once
// Physics (Minecraft tick values @20 TPS), raycasts, mobs.
#include "world.hpp"
#include <random>

inline std::mt19937& rng(){static std::mt19937 r{std::random_device{}()};return r;}
inline float frand(){return std::uniform_real_distribution<float>(0.f,1.f)(rng());}
inline int irand(int a,int b){return std::uniform_int_distribution<int>(a,b)(rng());}

struct Body{ Vector3 pos{0,0,0},vel{0,0,0}; float w=0.6f,h=1.8f; bool onGround=false,inWater=false,hitWall=false; };

inline bool boxHit(World&W,const Body&b){
  int x0=(int)floorf(b.pos.x-b.w/2),x1=(int)floorf(b.pos.x+b.w/2-1e-5f);
  int y0=(int)floorf(b.pos.y),y1=(int)floorf(b.pos.y+b.h-1e-5f);
  int z0=(int)floorf(b.pos.z-b.w/2),z1=(int)floorf(b.pos.z+b.w/2-1e-5f);
  for(int y=y0;y<=y1;y++)for(int z=z0;z<=z1;z++)for(int x=x0;x<=x1;x++) if(binfo(W.getBlock(x,y,z)).solid)return true;
  return false;
}
inline bool moveAxis(World&W,Body&b,int axis,float d){
  if(d==0)return false;
  int steps=(int)ceilf(fabsf(d)/0.4f); float sd=d/steps;
  float*p=axis==0?&b.pos.x:(axis==1?&b.pos.y:&b.pos.z);
  for(int i=0;i<steps;i++){
    float old=*p; *p+=sd;
    if(boxHit(W,b)){
      float lo=0,hi=1;
      for(int k=0;k<12;k++){float mid=(lo+hi)/2;*p=old+sd*mid;if(boxHit(W,b))hi=mid;else lo=mid;}
      *p=old+sd*lo; return true;
    }
  }
  return false;
}
// Java/Bedrock-style tick: accel on ground = speed*(0.16277136/f^3), f=0.546; air accel 0.02;
// gravity vy=(vy-0.08)*0.98; ground drag 0.546, air drag 0.91; jump 0.42; sprint-jump +0.2.
inline void tickBody(World&W,Body&b,float F,float S,float yaw,bool jump,bool sprint,float speed,bool fly=false,float upDown=0){
  float fx=sinf(yaw),fz=cosf(yaw),rx=-fz,rz=fx;
  float len=sqrtf(F*F+S*S); if(len>1){F/=len;S/=len;}
  float wx=F*fx+S*rx,wz=F*fz+S*rz;
  b.inWater=W.getBlock((int)floorf(b.pos.x),(int)floorf(b.pos.y+0.4f),(int)floorf(b.pos.z))==B_WATER;
  b.hitWall=false;
  float f=b.onGround?0.546f:0.91f;
  if(fly){
    float sp=sprint?1.0f:0.5f; b.vel.x=wx*sp;b.vel.z=wz*sp;b.vel.y=upDown*0.4f;
  } else {
    float acc;
    if(b.inWater)acc=0.02f; else if(b.onGround)acc=speed*(0.16277136f/(f*f*f)); else acc=sprint?0.026f:0.02f;
    b.vel.x+=wx*acc;b.vel.z+=wz*acc;
    if(jump){
      if(b.inWater)b.vel.y+=0.04f;
      else if(b.onGround){b.vel.y=0.42f;if(sprint){b.vel.x+=fx*0.2f;b.vel.z+=fz*0.2f;}}
    }
  }
  float dx=b.vel.x,dy=b.vel.y,dz=b.vel.z;
  bool cy=moveAxis(W,b,1,dy); b.onGround=cy&&dy<0; if(cy)b.vel.y=0;
  bool cx=moveAxis(W,b,0,dx), cz=moveAxis(W,b,2,dz);
  if(cx){b.vel.x=0;b.hitWall=true;} if(cz){b.vel.z=0;b.hitWall=true;}
  if(!fly){
    if(b.inWater){b.vel.x*=0.8f;b.vel.y*=0.8f;b.vel.z*=0.8f;b.vel.y-=0.02f;}
    else{b.vel.x*=f;b.vel.z*=f;b.vel.y=(b.vel.y-0.08f)*0.98f;}
  }
}

struct Hit{ bool hit=false; int x=0,y=0,z=0,nx=0,ny=0,nz=0; float dist=0; };
inline Hit raycast(World&W,Vector3 o,Vector3 d,float maxD){
  Hit h; int x=(int)floorf(o.x),y=(int)floorf(o.y),z=(int)floorf(o.z);
  int sx=d.x>0?1:-1,sy=d.y>0?1:-1,sz=d.z>0?1:-1;
  float tdx=d.x!=0?fabsf(1.0f/d.x):1e30f,tdy=d.y!=0?fabsf(1.0f/d.y):1e30f,tdz=d.z!=0?fabsf(1.0f/d.z):1e30f;
  float nx=d.x>0?(x+1-o.x)*tdx:(o.x-x)*tdx, ny=d.y>0?(y+1-o.y)*tdy:(o.y-y)*tdy, nz=d.z>0?(z+1-o.z)*tdz:(o.z-z)*tdz;
  float t=0; int ax=0,ay=0,az=0;
  while(t<=maxD){
    int b=W.getBlock(x,y,z);
    if(b!=B_AIR&&b!=B_WATER){h.hit=true;h.x=x;h.y=y;h.z=z;h.nx=ax;h.ny=ay;h.nz=az;h.dist=t;return h;}
    if(nx<ny&&nx<nz){t=nx;nx+=tdx;x+=sx;ax=-sx;ay=0;az=0;}
    else if(ny<nz){t=ny;ny+=tdy;y+=sy;ax=0;ay=-sy;az=0;}
    else{t=nz;nz+=tdz;z+=sz;ax=0;ay=0;az=-sz;}
  }
  return h;
}
inline bool rayBox(Vector3 o,Vector3 d,Vector3 mn,Vector3 mx,float&tOut){
  float t0=0,t1=1e9f; float oo[3]={o.x,o.y,o.z},dd[3]={d.x,d.y,d.z},a[3]={mn.x,mn.y,mn.z},b[3]={mx.x,mx.y,mx.z};
  for(int i=0;i<3;i++){
    if(fabsf(dd[i])<1e-8f){if(oo[i]<a[i]||oo[i]>b[i])return false;continue;}
    float ta=(a[i]-oo[i])/dd[i],tb=(b[i]-oo[i])/dd[i]; if(ta>tb)std::swap(ta,tb);
    t0=std::max(t0,ta);t1=std::min(t1,tb); if(t0>t1)return false;
  }
  tOut=t0; return true;
}

enum MobType{M_PIG=0,M_ZOMBIE=1};
struct Mob{ int type=0; Body b; Vector3 prev{0,0,0}; float yaw=0,wyaw=0; int hp=10,hurt=0,cd=0,ai=0; bool moving=false; };
inline Mob makeMob(int type,Vector3 p){
  Mob m; m.type=type; m.b.pos=p; m.prev=p;
  if(type==M_PIG){m.b.w=0.9f;m.b.h=0.9f;m.hp=10;} else {m.b.w=0.6f;m.b.h=1.8f;m.hp=20;}
  return m;
}
// returns melee damage dealt to the player this tick (0 = none)
inline int tickMob(World&W,Mob&m,Vector3 pp,bool hostile){
  m.prev=m.b.pos; if(m.hurt>0)m.hurt--; if(m.cd>0)m.cd--;
  float dx=pp.x-m.b.pos.x,dz=pp.z-m.b.pos.z,dy=pp.y-m.b.pos.y,d=sqrtf(dx*dx+dz*dz);
  if(m.type==M_ZOMBIE&&hostile&&d<24){
    m.yaw=m.wyaw=atan2f(dx,dz);
    tickBody(W,m.b,d>0.9f?1.0f:0.0f,0,m.yaw,m.b.hitWall||m.b.inWater,false,0.1f);
    if(d<1.1f&&fabsf(dy)<1.6f&&m.cd==0){m.cd=20;return 3;}
    return 0;
  }
  if(--m.ai<=0){m.moving=frand()<0.55f;m.wyaw=frand()*6.2831853f;m.ai=irand(30,90);}
  if(m.moving)m.yaw=m.wyaw;
  tickBody(W,m.b,m.moving?1.0f:0.0f,0,m.wyaw,(m.b.hitWall&&m.moving)||m.b.inWater,false,m.type==M_PIG?0.05f:0.04f);
  return 0;
}
