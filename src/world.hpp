#pragma once
// Voxel world: noise terrain, caves, ores, trees, chunk meshing.
#include "blocks.hpp"
#include "raymath.h"
#include <unordered_map>
#include <array>
#include <cstring>
#include <cmath>

constexpr int CW=16, CH=96, SEA=36;

inline uint32_t hash3(int x,int y,int z,uint32_t s){
  uint32_t h=s*2654435761u; h^=(uint32_t)x*374761393u; h=(h<<13|h>>19)*1274126177u;
  h^=(uint32_t)y*668265263u; h=(h^(h>>15))*2246822519u; h^=(uint32_t)z*3266489917u;
  h^=h>>16; h*=2246822519u; h^=h>>13; return h;
}
inline float rnd3(int x,int y,int z,uint32_t s){return (hash3(x,y,z,s)&0xFFFFFF)/16777215.0f;}
inline float sm(float t){return t*t*(3-2*t);}
inline float lerpf(float a,float b,float t){return a+(b-a)*t;}
inline float vnoise2(float x,float z,uint32_t s){
  int xi=(int)floorf(x),zi=(int)floorf(z); float fx=sm(x-xi),fz=sm(z-zi);
  float a=rnd3(xi,0,zi,s),b=rnd3(xi+1,0,zi,s),c=rnd3(xi,0,zi+1,s),d=rnd3(xi+1,0,zi+1,s);
  return lerpf(lerpf(a,b,fx),lerpf(c,d,fx),fz);
}
inline float vnoise3(float x,float y,float z,uint32_t s){
  int xi=(int)floorf(x),yi=(int)floorf(y),zi=(int)floorf(z); float fx=sm(x-xi),fy=sm(y-yi),fz=sm(z-zi);
  float c[2][2][2];
  for(int i=0;i<2;i++)for(int j=0;j<2;j++)for(int k=0;k<2;k++) c[i][j][k]=rnd3(xi+i,yi+j,zi+k,s);
  float x00=lerpf(c[0][0][0],c[1][0][0],fx),x10=lerpf(c[0][1][0],c[1][1][0],fx);
  float x01=lerpf(c[0][0][1],c[1][0][1],fx),x11=lerpf(c[0][1][1],c[1][1][1],fx);
  return lerpf(lerpf(x00,x10,fy),lerpf(x01,x11,fy),fz);
}
inline float fbm2(float x,float z,uint32_t s,int oct){
  float a=0.5f,sum=0,norm=0;
  for(int i=0;i<oct;i++){sum+=a*vnoise2(x,z,s+i*131);norm+=a;x*=2;z*=2;a*=0.5f;}
  return sum/norm;
}
inline int terrainHeight(int x,int z,uint32_t seed){
  float n=fbm2(x*0.006f,z*0.006f,seed,4), m=fbm2(x*0.02f,z*0.02f,seed+77,3);
  float h=SEA+2+(n-0.5f)*90.0f+(m-0.5f)*10.0f;
  return (int)std::clamp(h,4.0f,(float)CH-12.0f);
}

struct Chunk{ int cx=0,cz=0; uint8_t b[CW*CH*CW]; uint8_t top[CW*CW]; Mesh mesh{}; bool hasMesh=false,dirty=true; };
inline int cidx(int x,int y,int z){return (y*CW+z)*CW+x;}

static const int FD[6][3]={{1,0,0},{-1,0,0},{0,0,1},{0,0,-1},{0,1,0},{0,-1,0}};
static const float FV[6][4][5]={
 {{1,0,1,0,1},{1,0,0,1,1},{1,1,0,1,0},{1,1,1,0,0}},
 {{0,0,0,0,1},{0,0,1,1,1},{0,1,1,1,0},{0,1,0,0,0}},
 {{0,0,1,0,1},{1,0,1,1,1},{1,1,1,1,0},{0,1,1,0,0}},
 {{1,0,0,0,1},{0,0,0,1,1},{0,1,0,1,0},{1,1,0,0,0}},
 {{0,1,1,0,1},{1,1,1,1,1},{1,1,0,1,0},{0,1,0,0,0}},
 {{0,0,0,0,1},{1,0,0,1,1},{1,0,1,1,0},{0,0,1,0,0}}};
static const float FSH[6]={0.8f,0.8f,0.65f,0.65f,1.0f,0.5f};
static const int TRI[6]={0,1,2,0,2,3};

struct World{
  uint32_t seed=1; Material mat{};
  std::unordered_map<int64_t,Chunk*> chunks;
  std::unordered_map<int64_t,std::vector<std::array<int,4>>> edits; // per chunk: lx,y,lz,id
  std::vector<std::pair<int,int>> ring;

  World(){
    for(int dz=-14;dz<=14;dz++)for(int dx=-14;dx<=14;dx++) ring.push_back({dx,dz});
    std::sort(ring.begin(),ring.end(),[](const std::pair<int,int>&a,const std::pair<int,int>&b){return a.first*a.first+a.second*a.second<b.first*b.first+b.second*b.second;});
  }
  void initGfx(Texture2D atlas){mat=LoadMaterialDefault();mat.maps[MATERIAL_MAP_DIFFUSE].texture=atlas;}
  static int64_t key(int cx,int cz){return ((int64_t)cx<<32)^(uint32_t)cz;}
  Chunk* get(int cx,int cz){auto it=chunks.find(key(cx,cz));return it==chunks.end()?nullptr:it->second;}
  void freeChunk(Chunk*c){if(c->hasMesh)UnloadMesh(c->mesh);delete c;}
  void clear(){for(auto&kv:chunks)freeChunk(kv.second);chunks.clear();edits.clear();}

  int getBlock(int x,int y,int z){
    if(y<0)return B_BEDROCK; if(y>=CH)return B_AIR;
    Chunk*c=get(x>>4,z>>4); if(!c)return B_STONE;
    return c->b[cidx(x&15,y,z&15)];
  }
  int topY(int x,int z){Chunk*c=get(x>>4,z>>4);return c?c->top[(z&15)*16+(x&15)]:CH;}
  void recomputeTop(Chunk*c,int lx,int lz){
    int t=0; for(int y=CH-1;y>=0;y--) if(binfo(c->b[cidx(lx,y,lz)]).opaque){t=y;break;}
    c->top[lz*16+lx]=(uint8_t)t;
  }
  void dirtyAt(int cx,int cz){Chunk*c=get(cx,cz);if(c)c->dirty=true;}
  void setBlock(int x,int y,int z,int id){
    if(y<0||y>=CH)return;
    int cx=x>>4,cz=z>>4,lx=x&15,lz=z&15;
    edits[key(cx,cz)].push_back({lx,y,lz,id});
    Chunk*c=get(cx,cz); if(!c)return;
    c->b[cidx(lx,y,lz)]=(uint8_t)id; recomputeTop(c,lx,lz); c->dirty=true;
    if(lx==0)dirtyAt(cx-1,cz); if(lx==15)dirtyAt(cx+1,cz);
    if(lz==0)dirtyAt(cx,cz-1); if(lz==15)dirtyAt(cx,cz+1);
  }

  void generate(int cx,int cz){
    Chunk*c=new Chunk(); memset(c->b,0,sizeof(c->b)); memset(c->top,0,sizeof(c->top)); c->cx=cx;c->cz=cz;
    for(int lz=0;lz<16;lz++)for(int lx=0;lx<16;lx++){
      int wx=cx*16+lx,wz=cz*16+lz,h=terrainHeight(wx,wz,seed);
      bool beach=h<=SEA+1;
      for(int y=0;y<=h;y++){
        uint8_t id;
        if(y==0)id=B_BEDROCK;
        else if(y<h-3){id=B_STONE;if(rnd3(wx,y,wz,seed+5)<0.012f)id=B_COAL_ORE;else if(y<30&&rnd3(wx,y,wz,seed+9)<0.006f)id=B_IRON_ORE;}
        else if(y<h)id=beach?B_SAND:B_DIRT;
        else id=beach?B_SAND:B_GRASS;
        if(y>3){
          float a=vnoise3(wx*0.05f,y*0.07f,wz*0.05f,seed+21),b=vnoise3(wx*0.05f,y*0.07f,wz*0.05f,seed+22);
          bool tunnel=fabsf(a-0.5f)<0.035f&&fabsf(b-0.5f)<0.035f;
          bool cavern=y<h-6&&vnoise3(wx*0.03f,y*0.05f,wz*0.03f,seed+23)>0.74f;
          if(tunnel||cavern)id=B_AIR;
        }
        c->b[cidx(lx,y,lz)]=id;
      }
      for(int y=h+1;y<=SEA;y++)c->b[cidx(lx,y,lz)]=B_WATER;
    }
    for(int lz=2;lz<14;lz++)for(int lx=2;lx<14;lx++){
      int wx=cx*16+lx,wz=cz*16+lz,h=terrainHeight(wx,wz,seed);
      if(h>SEA+1&&h<CH-12&&c->b[cidx(lx,h,lz)]==B_GRASS&&rnd3(wx,0,wz,seed+3)<0.012f){
        int th=4+(int)(hash3(wx,1,wz,seed)%2);
        for(int y=h+1;y<=h+th;y++)c->b[cidx(lx,y,lz)]=B_LOG;
        for(int y=h+th-2;y<=h+th+1;y++){
          int r=(y>=h+th)?1:2;
          for(int dz=-r;dz<=r;dz++)for(int dx=-r;dx<=r;dx++){
            if(abs(dx)==r&&abs(dz)==r&&rnd3(wx+dx,y,wz+dz,seed+4)<0.5f)continue;
            uint8_t&bb=c->b[cidx(lx+dx,y,lz+dz)]; if(bb==B_AIR)bb=B_LEAVES;
          }
        }
      }
    }
    auto it=edits.find(key(cx,cz));
    if(it!=edits.end())for(auto&e:it->second)c->b[cidx(e[0],e[1],e[2])]=(uint8_t)e[3];
    for(int lz=0;lz<16;lz++)for(int lx=0;lx<16;lx++)recomputeTop(c,lx,lz);
    chunks[key(cx,cz)]=c;
    dirtyAt(cx-1,cz);dirtyAt(cx+1,cz);dirtyAt(cx,cz-1);dirtyAt(cx,cz+1);
  }

  void buildMesh(Chunk*c){
    std::vector<float> V,UV; std::vector<unsigned char> C; V.reserve(60000);UV.reserve(40000);C.reserve(80000);
    int bx=c->cx*16,bz=c->cz*16;
    auto nbBlock=[&](int lx,int y,int lz)->int{
      if(y<0)return B_BEDROCK; if(y>=CH)return B_AIR;
      if(lx>=0&&lx<16&&lz>=0&&lz<16)return c->b[cidx(lx,y,lz)];
      return getBlock(bx+lx,y,bz+lz);
    };
    auto nbTop=[&](int lx,int lz)->int{
      if(lx>=0&&lx<16&&lz>=0&&lz<16)return c->top[lz*16+lx];
      return topY(bx+lx,bz+lz);
    };
    for(int y=0;y<CH;y++)for(int lz=0;lz<16;lz++)for(int lx=0;lx<16;lx++){
      int id=c->b[cidx(lx,y,lz)]; if(!id)continue; const BlockInfo&bi=binfo(id);
      for(int f=0;f<6;f++){
        int nx=lx+FD[f][0],ny=y+FD[f][1],nz=lz+FD[f][2];
        int nb=nbBlock(nx,ny,nz);
        if(binfo(nb).opaque)continue;
        if(nb==id&&(id==B_WATER||id==B_GLASS))continue;
        bool lit=ny>=CH||ny>nbTop(nx,nz);
        float sh=FSH[f]*(lit?1.0f:0.35f);
        unsigned char cr=(unsigned char)(255*sh),al=(id==B_WATER)?160:255;
        int tile=f==4?bi.top:(f==5?bi.bot:bi.side); int tc=tile%8,tr=tile/8;
        for(int k=0;k<6;k++){
          const float*v=FV[f][TRI[k]];
          V.push_back(bx+lx+v[0]);V.push_back(y+v[1]);V.push_back(bz+lz+v[2]);
          UV.push_back((tc+(v[3]>0.5f?0.99f:0.01f))/8.0f);UV.push_back((tr+(v[4]>0.5f?0.99f:0.01f))/3.0f);
          C.push_back(cr);C.push_back(cr);C.push_back(cr);C.push_back(al);
        }
      }
    }
    if(c->hasMesh){UnloadMesh(c->mesh);c->hasMesh=false;c->mesh=Mesh{};}
    c->dirty=false;
    int n=(int)(V.size()/3); if(n==0)return;
    Mesh m{}; m.vertexCount=n; m.triangleCount=n/3;
    m.vertices=(float*)MemAlloc(n*3*sizeof(float)); memcpy(m.vertices,V.data(),n*3*sizeof(float));
    m.texcoords=(float*)MemAlloc(n*2*sizeof(float)); memcpy(m.texcoords,UV.data(),n*2*sizeof(float));
    m.colors=(unsigned char*)MemAlloc(n*4); memcpy(m.colors,C.data(),n*4);
    UploadMesh(&m,false); c->mesh=m; c->hasMesh=true;
  }

  void update(int pcx,int pcz,int R,int genBudget,int meshBudget){
    for(auto&o:ring){
      int d=std::max(abs(o.first),abs(o.second)); if(d>R+1)continue;
      if(!get(pcx+o.first,pcz+o.second)){generate(pcx+o.first,pcz+o.second);if(--genBudget<=0)break;}
    }
    for(auto&o:ring){
      int d=std::max(abs(o.first),abs(o.second)); if(d>R)continue;
      Chunk*c=get(pcx+o.first,pcz+o.second);
      if(c&&c->dirty&&get(c->cx+1,c->cz)&&get(c->cx-1,c->cz)&&get(c->cx,c->cz+1)&&get(c->cx,c->cz-1)){
        buildMesh(c); if(--meshBudget<=0)break;
      }
    }
    for(auto it=chunks.begin();it!=chunks.end();){
      Chunk*c=it->second;
      if(std::max(abs(c->cx-pcx),abs(c->cz-pcz))>R+3){freeChunk(c);it=chunks.erase(it);}else ++it;
    }
  }
  void draw(int pcx,int pcz,int R){
    for(auto&kv:chunks){
      Chunk*c=kv.second; if(!c->hasMesh)continue;
      if(std::max(abs(c->cx-pcx),abs(c->cz-pcz))>R)continue;
      DrawMesh(c->mesh,mat,MatrixIdentity());
    }
  }
};
