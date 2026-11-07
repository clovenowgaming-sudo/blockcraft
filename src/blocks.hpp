#pragma once
// Blocks, items, tools, recipes, inventory stacks.
#include "raylib.h"
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

enum BlockId : uint8_t { B_AIR=0,B_STONE,B_GRASS,B_DIRT,B_COBBLE,B_PLANKS,B_LOG,B_LEAVES,B_SAND,B_WATER,B_BEDROCK,B_COAL_ORE,B_IRON_ORE,B_TABLE,B_GLASS,B_COUNT };
enum ItemId { I_STICK=100,I_COAL,I_IRON_INGOT,I_PORK,I_FLESH };
enum ToolType { T_PICK=0,T_AXE,T_SHOVEL,T_SWORD,T_HOE };

struct BlockInfo{ const char* name; int top,side,bot; float hard; bool solid,opaque; int tool,needTier,drop,dropN; Color col; };

inline const BlockInfo& binfo(int id){
  static const BlockInfo T[B_COUNT]={
   {"Air",0,0,0,0,false,false,-1,-1,0,0,{0,0,0,0}},
   {"Stone",0,0,0,1.5f,true,true,T_PICK,0,B_COBBLE,1,{125,125,125,255}},
   {"Grass",1,2,3,0.6f,true,true,T_SHOVEL,-1,B_DIRT,1,{95,159,53,255}},
   {"Dirt",3,3,3,0.5f,true,true,T_SHOVEL,-1,B_DIRT,1,{134,96,67,255}},
   {"Cobblestone",4,4,4,2.0f,true,true,T_PICK,0,B_COBBLE,1,{110,110,110,255}},
   {"Planks",5,5,5,2.0f,true,true,T_AXE,-1,B_PLANKS,1,{157,128,79,255}},
   {"Log",6,7,6,2.0f,true,true,T_AXE,-1,B_LOG,1,{102,81,50,255}},
   {"Leaves",8,8,8,0.2f,true,true,-1,-1,0,0,{60,130,40,255}},
   {"Sand",9,9,9,0.5f,true,true,T_SHOVEL,-1,B_SAND,1,{219,207,142,255}},
   {"Water",10,10,10,-1.0f,false,false,-1,-1,0,0,{50,90,200,255}},
   {"Bedrock",11,11,11,-1.0f,true,true,-1,-1,0,0,{60,60,60,255}},
   {"Coal Ore",12,12,12,3.0f,true,true,T_PICK,0,I_COAL,1,{80,80,80,255}},
   {"Iron Ore",13,13,13,3.0f,true,true,T_PICK,1,I_IRON_INGOT,1,{170,140,120,255}},
   {"Crafting Table",14,15,5,2.5f,true,true,T_AXE,-1,B_TABLE,1,{140,100,50,255}},
   {"Glass",16,16,16,0.3f,true,false,-1,-1,0,0,{200,230,240,255}},
  };
  return T[id];
}

inline bool isTool(int id){return id>=110&&id<125;}
inline int toolType(int id){return isTool(id)?(id-110)/3:-1;}
inline int toolTier(int id){return isTool(id)?(id-110)%3:-1;}
inline int toolId(int type,int tier){return 110+type*3+tier;}
inline int toolMaxDur(int id){static const int d[3]={59,131,250};return isTool(id)?d[toolTier(id)]:0;}
inline int maxStack(int id){return isTool(id)?1:64;}
inline int foodValue(int id){return id==I_PORK?3:(id==I_FLESH?4:0);}

inline std::string itemName(int id){
  if(id>0&&id<B_COUNT) return binfo(id).name;
  switch(id){case I_STICK:return "Stick";case I_COAL:return "Coal";case I_IRON_INGOT:return "Iron Ingot";case I_PORK:return "Raw Porkchop";case I_FLESH:return "Rotten Flesh";}
  if(isTool(id)){static const char*tn[3]={"Wooden","Stone","Iron"};static const char*ty[5]={"Pickaxe","Axe","Shovel","Sword","Hoe"};return std::string(tn[toolTier(id)])+" "+ty[toolType(id)];}
  return "?";
}
inline std::string itemAbbr(int id){
  if(isTool(id)){static const char*ab[5]={"Pk","Ax","Sh","Sw","Ho"};return ab[toolType(id)];}
  std::string n=itemName(id); return n.substr(0,2);
}
inline Color itemColor(int id){
  if(id>0&&id<B_COUNT) return binfo(id).col;
  switch(id){case I_STICK:return Color{140,100,50,255};case I_COAL:return Color{40,40,40,255};case I_IRON_INGOT:return Color{220,220,220,255};case I_PORK:return Color{240,150,150,255};case I_FLESH:return Color{120,150,70,255};}
  if(isTool(id)){static const Color c[3]={{157,128,79,255},{130,130,130,255},{225,225,225,255}};return c[toolTier(id)];}
  return Color{255,0,255,255};
}
inline float weaponDamage(int id){
  if(!isTool(id)) return 1;
  int t=toolType(id),r=toolTier(id);
  switch(t){case T_SWORD:return 4.f+r;case T_AXE:return 3.f+r;case T_PICK:return 2.f+r;case T_SHOVEL:return 2.f+r;default:return 1.f;}
}
// Mining: damage per tick = speed / hardness / (30 if harvestable else 100)
inline float toolSpeed(int held,int need){return (isTool(held)&&toolType(held)==need)?2.0f*(toolTier(held)+1):1.0f;}
inline bool canHarvest(const BlockInfo&bi,int held){
  if(bi.needTier<0) return true;
  return isTool(held)&&toolType(held)==bi.tool&&toolTier(held)>=bi.needTier;
}

struct Stack{ int id=0,n=0,dur=0; bool empty()const{return id==0||n<=0;} };

inline int addToInv(Stack*s,int count,int id){ // 36 slots, returns leftover
  int mx=maxStack(id);
  if(mx>1) for(int i=0;i<36&&count>0;i++) if(s[i].id==id&&s[i].n<mx){int k=std::min(mx-s[i].n,count);s[i].n+=k;count-=k;}
  for(int i=0;i<36&&count>0;i++) if(s[i].empty()){int k=std::min(mx,count);s[i]=Stack{id,k,toolMaxDur(id)};count-=k;}
  return count;
}

struct Recipe{int w,h;std::vector<int> p;int out,cnt;};
inline const std::vector<Recipe>& recipes(){
  static std::vector<Recipe> R;
  if(R.empty()){
    R.push_back({1,1,{B_LOG},B_PLANKS,4});
    R.push_back({1,2,{B_PLANKS,B_PLANKS},I_STICK,4});
    R.push_back({2,2,{B_PLANKS,B_PLANKS,B_PLANKS,B_PLANKS},B_TABLE,1});
    R.push_back({2,2,{B_SAND,B_SAND,B_SAND,B_SAND},B_GLASS,4});
    int mats[3]={B_PLANKS,B_COBBLE,I_IRON_INGOT};
    for(int t=0;t<3;t++){
      int M=mats[t],S=I_STICK;
      R.push_back({3,3,{M,M,M,0,S,0,0,S,0},toolId(T_PICK,t),1});
      R.push_back({2,3,{M,M,M,S,0,S},toolId(T_AXE,t),1});
      R.push_back({1,3,{M,S,S},toolId(T_SHOVEL,t),1});
      R.push_back({1,3,{M,M,S},toolId(T_SWORD,t),1});
      R.push_back({2,3,{M,M,0,S,0,S},toolId(T_HOE,t),1});
    }
  }
  return R;
}
inline const Recipe* matchRecipe(const Stack*g){ // g = 3x3 array (2x2 uses top-left)
  int minr=3,maxr=-1,minc=3,maxc=-1;
  for(int r=0;r<3;r++)for(int c=0;c<3;c++) if(!g[r*3+c].empty()){minr=std::min(minr,r);maxr=std::max(maxr,r);minc=std::min(minc,c);maxc=std::max(maxc,c);}
  if(maxr<0) return nullptr;
  int w=maxc-minc+1,h=maxr-minr+1;
  std::vector<int> p;
  for(int r=minr;r<=maxr;r++)for(int c=minc;c<=maxc;c++) p.push_back(g[r*3+c].empty()?0:g[r*3+c].id);
  for(auto&R:recipes()){
    if(R.w!=w||R.h!=h) continue;
    if(R.p==p) return &R;
    std::vector<int> m(p);
    for(int r=0;r<h;r++)for(int c=0;c<w;c++) m[r*w+c]=p[r*w+(w-1-c)];
    if(R.p==m) return &R;
  }
  return nullptr;
}
