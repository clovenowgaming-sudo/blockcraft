// BlockCraft PE - original voxel sandbox (C++17 + raylib 5.0). Not affiliated with Mojang/Microsoft.
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "entity.hpp"
#include "audio.hpp"
#include "net.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <map>

namespace fs=std::filesystem;
static Color rgba(int r,int g,int b,int a=255){return Color{(unsigned char)std::clamp(r,0,255),(unsigned char)std::clamp(g,0,255),(unsigned char)std::clamp(b,0,255),(unsigned char)std::clamp(a,0,255)};}
static Rectangle rc(float x,float y,float w,float h){return Rectangle{x,y,w,h};}
static Vector3 lerp3(Vector3 a,Vector3 b,float t){return Vector3{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}

enum Screen{S_MENU,S_WORLDS,S_CREATE,S_SERVERS,S_ADDSRV,S_SETTINGS,S_LOADING,S_CONNECTING,S_PLAY};
struct WorldMeta{std::string name="World";uint32_t seed=0;int mode=0;};
struct ServerEntry{std::string name,addr;};
struct Remote{std::string name;Vector3 pos{0,0,0},tgt{0,0,0};float yaw=0;};

struct Game{
  Screen scr=S_MENU; bool quit=false;
  int rd=5; bool music=true; float fov=70; std::string playerName="Player";
  Texture2D atlas{}; World world; WorldMeta meta;
  Body pl; Vector3 prev{0,0,0},spawn{0,0,0},eye{0,0,0}; float yaw=0,pitch=0; int mode=0;
  int health=20,hunger=20,hungerT=0,regenT=0,invuln=0; float fall=0; bool flying=false;
  Stack inv[36]; Stack craft[9]; Stack cursor; int sel=0; std::string tip;
  bool invOpen=false,table=false,paused=false,dead=false,debug=false,cursorFree=true;
  std::vector<Mob> mobs; long tickN=0,worldT=1000; float acc=0;
  Hit tgt; int tgtMob=-1; int mx=0,my=0,mz=0; float mineProg=0; int mineCD=0; float placeCD=0;
  bool mp=false; NetClient net; int myId=0; std::map<int,Remote> remotes; std::vector<std::string> chat,backlog;
  bool chatOpen=false; std::string chatBuf; int posT=0; float chatShow=0;
  std::vector<WorldMeta> worlds; std::vector<ServerEntry> servers; int selServer=-1;
  std::string fName="New World",fSeed,fSrvName,fSrvAddr,msg; int createMode=0,activeField=-1;
  int loadFrames=0,connectDelay=0; std::string pendingAddr; Camera3D cam{};
};
static Game G;
static Synth synth; static Sound sDig,sPlace,sHurt; static bool sfxOk=false;
static void playSfx(Sound&s){if(sfxOk)PlaySound(s);}

// ---------- UI helpers (logical coords, 300 units tall) ----------
static float UIS=1; static Vector2 M{0,0}; static bool clk=false,rclk=false;
static float LW(){return GetScreenWidth()/UIS;} static float LH(){return GetScreenHeight()/UIS;}
static void txt(const std::string&s,float x,float y,int sz,Color c=WHITE,bool shadow=true){
  if(shadow)DrawText(s.c_str(),(int)x+1,(int)y+1,sz,rgba(30,30,30,200));
  DrawText(s.c_str(),(int)x,(int)y,sz,c);
}
static float tw(const std::string&s,int sz){return (float)MeasureText(s.c_str(),sz);}
static void ctxt(const std::string&s,float cx,float y,int sz,Color c=WHITE){txt(s,cx-tw(s,sz)/2,y,sz,c);}
static Rectangle tileRect(int t){return rc((float)(t%8*16),(float)(t/8*16),16,16);}
static void dirtBG(){
  for(float y=0;y<LH();y+=24)for(float x=0;x<LW();x+=24)DrawTexturePro(G.atlas,tileRect(3),rc(x,y,24,24),Vector2{0,0},0,rgba(90,90,90));
}
static bool button(Rectangle r,const std::string&l,bool en=true){
  bool hov=en&&CheckCollisionPointRec(M,r);
  DrawRectangleRec(r,en?(hov?rgba(100,110,170):rgba(115,115,115)):rgba(65,65,65));
  DrawRectangle((int)r.x,(int)r.y,(int)r.width,1,rgba(190,190,190));
  DrawRectangle((int)r.x,(int)r.y,1,(int)r.height,rgba(190,190,190));
  DrawRectangle((int)r.x,(int)(r.y+r.height-1),(int)r.width,1,rgba(40,40,40));
  DrawRectangle((int)(r.x+r.width-1),(int)r.y,1,(int)r.height,rgba(40,40,40));
  ctxt(l,r.x+r.width/2,r.y+r.height/2-5,10,en?(hov?rgba(255,255,160):WHITE):rgba(150,150,150));
  if(hov&&clk){clk=false;return true;}
  return false;
}
static void field(int id,Rectangle r,std::string&s,const char*hint,size_t mx=24){
  bool hov=CheckCollisionPointRec(M,r);
  if(clk){if(hov){G.activeField=id;clk=false;}else if(G.activeField==id)G.activeField=-1;}
  DrawRectangleRec(r,rgba(15,15,15)); DrawRectangleLinesEx(r,1,G.activeField==id?WHITE:rgba(160,160,160));
  std::string shown=s; if(G.activeField==id&&((int)(GetTime()*2)%2==0))shown+="_";
  if(s.empty()&&G.activeField!=id)txt(hint,r.x+4,r.y+r.height/2-5,10,rgba(120,120,120)); else txt(shown,r.x+4,r.y+r.height/2-5,10);
  if(G.activeField==id){
    int ch; while((ch=GetCharPressed())>0) if(ch>=32&&ch<127&&s.size()<mx)s+=(char)ch;
    if((IsKeyPressed(KEY_BACKSPACE)||IsKeyPressedRepeat(KEY_BACKSPACE))&&!s.empty())s.pop_back();
  }
}

// ---------- procedural texture atlas (original art, 8x3 tiles of 16px) ----------
static void makeAtlas(){
  Image img=GenImageColor(128,48,rgba(0,0,0,0));
  static const int base[17][3]={{125,125,125},{95,159,53},{134,96,67},{134,96,67},{112,112,112},{157,128,79},{150,120,70},{102,81,50},{58,130,40},{219,207,142},{50,90,200},{55,55,55},{125,125,125},{125,125,125},{160,120,60},{130,95,50},{200,230,240}};
  for(int t=0;t<17;t++)for(int y=0;y<16;y++)for(int x=0;x<16;x++){
    float n=0.85f+0.3f*rnd3(x,y,t,99); int r=base[t][0],g=base[t][1],b=base[t][2],a=255;
    if(t==2&&y<4){r=95;g=159;b=53;}
    if(t==4&&((x/5+y/5)%2==0)){r-=18;g-=18;b-=18;}
    if(t==5&&y%4==3){r-=35;g-=35;b-=35;}
    if(t==6){float d=hypotf(x-7.5f,y-7.5f);if(((int)d)%3==0){r-=30;g-=30;b-=30;}}
    if(t==7&&x%4==0){r-=25;g-=25;b-=25;}
    if(t==8&&rnd3(x,y,t,7)<0.3f){r-=25;g-=30;b-=20;}
    if(t==12&&rnd3(x/2,y/2,t,5)<0.22f){r=30;g=30;b=30;n=1;}
    if(t==13&&rnd3(x/2,y/2,t,5)<0.22f){r=210;g=170;b=140;n=1;}
    if(t==14&&(x%8==0||y%8==0)){r-=50;g-=50;b-=50;}
    if(t==15&&y<3){r-=40;g-=40;b-=40;}
    if(t==16){a=(x==0||y==0||x==15||y==15)?255:90;n=1;}
    ImageDrawPixel(&img,(t%8)*16+x,(t/8)*16+y,rgba((int)(r*n),(int)(g*n),(int)(b*n),a));
  }
  G.atlas=LoadTextureFromImage(img); SetTextureFilter(G.atlas,TEXTURE_FILTER_POINT); UnloadImage(img);
}

// ---------- persistence ----------
static std::string savePath(const std::string&n){return "saves/"+n+".bcw";}
static void saveWorld(){
  if(G.mp)return; fs::create_directories("saves"); std::ofstream f(savePath(G.meta.name));
  f<<"BCW1\nseed "<<G.world.seed<<"\nmode "<<G.mode<<"\ntime "<<G.worldT<<"\npos "<<G.pl.pos.x<<" "<<G.pl.pos.y<<" "<<G.pl.pos.z<<" "<<G.yaw<<" "<<G.pitch<<"\nstats "<<G.health<<" "<<G.hunger<<"\n";
  for(int i=0;i<36;i++)f<<"inv "<<i<<" "<<G.inv[i].id<<" "<<G.inv[i].n<<" "<<G.inv[i].dur<<"\n";
  for(auto&kv:G.world.edits){
    int cx=(int)(kv.first>>32),cz=(int)(uint32_t)(kv.first&0xFFFFFFFF);
    for(auto&e:kv.second)f<<"e "<<cx*16+e[0]<<" "<<e[1]<<" "<<cz*16+e[2]<<" "<<e[3]<<"\n";
  }
}
static void loadSave(const std::string&name){
  std::ifstream f(savePath(name)); std::string tag,k; f>>tag; if(tag!="BCW1")return;
  while(f>>k){
    if(k=="seed"){uint32_t s;f>>s;G.world.seed=s;}
    else if(k=="mode")f>>G.mode; else if(k=="time")f>>G.worldT;
    else if(k=="pos"){f>>G.pl.pos.x>>G.pl.pos.y>>G.pl.pos.z>>G.yaw>>G.pitch;G.prev=G.pl.pos;}
    else if(k=="stats")f>>G.health>>G.hunger;
    else if(k=="inv"){int i,id,n,d;f>>i>>id>>n>>d;if(i>=0&&i<36)G.inv[i]=Stack{id,n,d};}
    else if(k=="e"){int x,y,z,id;f>>x>>y>>z>>id;G.world.setBlock(x,y,z,id);}
  }
}
static void refreshWorlds(){
  G.worlds.clear(); if(!fs::exists("saves"))return;
  for(auto&e:fs::directory_iterator("saves")){
    if(e.path().extension()!=".bcw")continue;
    std::ifstream f(e.path()); std::string tag,k; WorldMeta m; m.name=e.path().stem().string();
    f>>tag>>k>>m.seed>>k>>m.mode; G.worlds.push_back(m);
  }
}
static void loadServers(){
  G.servers.clear(); std::ifstream f("servers.txt"); std::string l;
  while(std::getline(f,l)){auto p=l.find('|');if(p==std::string::npos)continue;G.servers.push_back({l.substr(0,p),l.substr(p+1)});}
  if(G.servers.empty())G.servers.push_back({"Local Server","127.0.0.1:25566"});
}
static void saveServers(){std::ofstream f("servers.txt");for(auto&s:G.servers)f<<s.name<<"|"<<s.addr<<"\n";}

// ---------- world / player lifecycle ----------
static float bright(){float a=(float)(G.worldT%24000)/24000.0f*6.2831853f;return std::clamp(0.5f+0.8f*sinf(a),0.12f,1.0f);}
static Vector3 lookDir(){float cp=cosf(G.pitch);return Vector3{cp*sinf(G.yaw),sinf(G.pitch),cp*cosf(G.yaw)};}
static Vector3 findSpawn(uint32_t seed){
  for(int r=0;r<400;r+=6)for(int a=0;a<8;a++){
    float an=a*0.7853982f; int x=(int)(cosf(an)*r),z=(int)(sinf(an)*r); int h=terrainHeight(x,z,seed);
    if(h>SEA+2)return Vector3{x+0.5f,(float)h+2.0f,z+0.5f};
  }
  return Vector3{0.5f,(float)(SEA+10),0.5f};
}
static void resetPlayer(){
  G.pl=Body(); G.pl.pos=G.spawn; G.prev=G.spawn; G.yaw=0;G.pitch=0; G.health=20;G.hunger=20;G.fall=0;G.flying=false;
  G.dead=false;G.paused=false;G.invOpen=false;G.table=false;G.chatOpen=false;G.mineProg=0;G.sel=0;G.acc=0;G.worldT=1000;
  for(auto&s:G.inv)s=Stack(); for(auto&s:G.craft)s=Stack(); G.cursor=Stack();
  G.mobs.clear();G.remotes.clear();G.chat.clear();
}
static void startWorld(const WorldMeta&m,bool load,bool mp){
  G.world.clear(); G.meta=m; G.world.seed=m.seed; G.mode=m.mode; G.mp=mp;
  G.spawn=findSpawn(m.seed); resetPlayer(); if(load)loadSave(m.name);
  G.loadFrames=0; G.scr=S_LOADING;
}
static void leaveToMenu(const std::string&msg=""){
  saveWorld(); G.net.disconnect(); G.mp=false; G.world.clear(); G.mobs.clear(); G.remotes.clear();
  G.invOpen=G.paused=G.dead=G.chatOpen=false; EnableCursor(); G.cursorFree=true; G.msg=msg; G.scr=S_MENU;
}

// ---------- inventory logic ----------
static void closeInv(){
  for(auto&c:G.craft){if(!c.empty())addToInv(G.inv,c.n,c.id);c=Stack();}
  if(!G.cursor.empty()){addToInv(G.inv,G.cursor.n,G.cursor.id);G.cursor=Stack();}
  G.invOpen=false;G.table=false;
}
static void slotClick(Stack&s,bool right){
  Stack&c=G.cursor;
  if(!right){
    if(!c.empty()&&!s.empty()&&c.id==s.id&&maxStack(s.id)>1){int k=std::min(maxStack(s.id)-s.n,c.n);s.n+=k;c.n-=k;if(c.n<=0)c=Stack();}
    else std::swap(c,s);
  } else {
    if(c.empty()){if(!s.empty()){int k=(s.n+1)/2;c=Stack{s.id,k,s.dur};s.n-=k;if(s.n<=0)s=Stack();}}
    else if(s.empty()){s=Stack{c.id,1,c.dur};c.n--;if(c.n<=0)c=Stack();}
    else if(s.id==c.id&&s.n<maxStack(s.id)){s.n++;c.n--;if(c.n<=0)c=Stack();}
    else std::swap(c,s);
  }
}
static void takeOutput(const Recipe*R){
  Stack&c=G.cursor;
  if(c.empty()||(c.id==R->out&&c.n+R->cnt<=maxStack(R->out))){
    if(c.empty())c=Stack{R->out,R->cnt,toolMaxDur(R->out)}; else c.n+=R->cnt;
    for(auto&s:G.craft)if(!s.empty()){s.n--;if(s.n<=0)s=Stack();}
  }
}
static void drawStack(const Stack&s,float x,float y){
  if(s.empty())return;
  DrawRectangle((int)x+3,(int)y+3,12,12,itemColor(s.id)); DrawRectangleLines((int)x+3,(int)y+3,12,12,rgba(0,0,0,160));
  txt(itemAbbr(s.id),x+4,y+5,6);
  if(s.n>1){std::string n=std::to_string(s.n);txt(n,x+17-tw(n,6),y+11,6);}
  if(isTool(s.id)){float f=(float)s.dur/toolMaxDur(s.id);DrawRectangle((int)x+3,(int)y+15,(int)(12*f),1,f>0.5f?GREEN:(f>0.2f?YELLOW:RED));}
}
static void invUI(){
  DrawRectangle(0,0,(int)LW(),(int)LH(),rgba(0,0,0,140));
  float ox=(LW()-176)/2,oy=(LH()-166)/2; if(G.mode==1)ox-=50;
  DrawRectangleRec(rc(ox,oy,176,166),rgba(198,198,198)); DrawRectangleLinesEx(rc(ox,oy,176,166),1,rgba(40,40,40));
  txt(G.table?"Crafting Table":"Crafting",ox+8,oy+6,8,rgba(60,60,60),false);
  G.tip.clear();
  auto slot=[&](Stack&s,float x,float y)->bool{
    Rectangle r=rc(x,y,18,18); bool hov=CheckCollisionPointRec(M,r);
    DrawRectangleRec(r,rgba(139,139,139)); DrawRectangleLinesEx(r,1,rgba(55,55,55));
    if(hov)DrawRectangleRec(rc(x+1,y+1,16,16),rgba(255,255,255,90));
    drawStack(s,x,y); if(hov&&!s.empty())G.tip=itemName(s.id); return hov;
  };
  for(int i=9;i<36;i++){int k=i-9; if(slot(G.inv[i],ox+8+(k%9)*18,oy+84+(k/9)*18)&&(clk||rclk))slotClick(G.inv[i],rclk);}
  for(int i=0;i<9;i++) if(slot(G.inv[i],ox+8+i*18,oy+142)&&(clk||rclk))slotClick(G.inv[i],rclk);
  int n=G.table?3:2; float gx=G.table?ox+30:ox+88,gy=G.table?oy+17:oy+26;
  for(int r=0;r<n;r++)for(int c=0;c<n;c++) if(slot(G.craft[r*3+c],gx+c*18,gy+r*18)&&(clk||rclk))slotClick(G.craft[r*3+c],rclk);
  const Recipe*R=matchRecipe(G.craft); Stack out; if(R)out=Stack{R->out,R->cnt,toolMaxDur(R->out)};
  txt("->",ox+105,oy+40,10,rgba(60,60,60),false);
  if(slot(out,ox+130,oy+35)&&clk&&R)takeOutput(R);
  if(G.mode==1){
    static std::vector<int> pal;
    if(pal.empty()){for(int i=1;i<B_COUNT;i++)if(i!=B_WATER&&i!=B_BEDROCK)pal.push_back(i);for(int i=I_STICK;i<=I_FLESH;i++)pal.push_back(i);for(int i=110;i<125;i++)pal.push_back(i);}
    float px=ox+182,py=oy; DrawRectangleRec(rc(px,py,6*18+8,166),rgba(198,198,198)); txt("Creative",px+4,py+4,8,rgba(60,60,60),false);
    for(size_t i=0;i<pal.size();i++){
      Stack s{pal[i],1,toolMaxDur(pal[i])};
      if(slot(s,px+4+(i%6)*18,py+16+(i/6)*18)&&clk){ if(G.cursor.empty())G.cursor=Stack{pal[i],maxStack(pal[i]),toolMaxDur(pal[i])}; else G.cursor=Stack(); }
    }
  }
  if(!G.cursor.empty())drawStack(G.cursor,M.x-9,M.y-9);
  if(!G.tip.empty()&&G.cursor.empty())txt(G.tip,M.x+8,M.y-10,8);
}

// ---------- gameplay ----------
static void hurtPlayer(int dmg,const Vector3*from){
  if(G.mode==1||G.invuln>0||G.dead)return;
  G.health-=dmg; G.invuln=10; playSfx(sHurt);
  if(from){float dx=G.pl.pos.x-from->x,dz=G.pl.pos.z-from->z,l=sqrtf(dx*dx+dz*dz)+1e-4f;G.pl.vel.x+=dx/l*0.4f;G.pl.vel.z+=dz/l*0.4f;G.pl.vel.y=0.36f;}
  if(G.health<=0){G.health=0;G.dead=true;if(G.invOpen)closeInv();}
}
static void placeNet(int x,int y,int z,int id){
  G.world.setBlock(x,y,z,id);
  if(G.mp)G.net.sendLine("SET "+std::to_string(x)+" "+std::to_string(y)+" "+std::to_string(z)+" "+std::to_string(id));
}
static void useTool(){
  Stack&s=G.inv[G.sel];
  if(G.mode==0&&isTool(s.id)){if(--s.dur<=0)s=Stack();}
}
static void breakBlock(int x,int y,int z){
  int id=G.world.getBlock(x,y,z); const BlockInfo&bi=binfo(id); int held=G.inv[G.sel].id;
  placeNet(x,y,z,B_AIR); playSfx(sDig);
  if(G.mode==0){
    if(canHarvest(bi,held)&&bi.dropN>0)addToInv(G.inv,bi.dropN,bi.drop);
    useTool();
  }
}
static void mineTick(){
  int id=G.world.getBlock(G.tgt.x,G.tgt.y,G.tgt.z); const BlockInfo&bi=binfo(id); if(bi.hard<0){G.mineProg=0;return;}
  if(G.mode==1){if(G.mineCD<=0){breakBlock(G.tgt.x,G.tgt.y,G.tgt.z);G.mineCD=5;}return;}
  if(G.tgt.x!=G.mx||G.tgt.y!=G.my||G.tgt.z!=G.mz){G.mx=G.tgt.x;G.my=G.tgt.y;G.mz=G.tgt.z;G.mineProg=0;}
  int held=G.inv[G.sel].id; bool ch=canHarvest(bi,held);
  G.mineProg+=toolSpeed(held,bi.tool)/bi.hard/(ch?30.0f:100.0f);
  if(G.mineProg>=1.0f){breakBlock(G.tgt.x,G.tgt.y,G.tgt.z);G.mineProg=0;}
}
static void spawnMobs(){
  if(G.mp||G.tickN%40)return;
  int pigs=0,zs=0; for(auto&m:G.mobs)(m.type==M_PIG?pigs:zs)++;
  float ang=frand()*6.2831853f,dist=16+frand()*16;
  int x=(int)floorf(G.pl.pos.x+cosf(ang)*dist),z=(int)floorf(G.pl.pos.z+sinf(ang)*dist);
  int top=G.world.topY(x,z); if(top>=CH||top<=0)return;
  int b=G.world.getBlock(x,top,z); if(b==B_WATER||b==B_LEAVES)return;
  Vector3 p{x+0.5f,(float)top+1.0f,z+0.5f};
  if(bright()<0.4f&&G.mode==0&&zs<8&&frand()<0.6f)G.mobs.push_back(makeMob(M_ZOMBIE,p));
  else if(pigs<8&&b==B_GRASS&&frand()<0.5f)G.mobs.push_back(makeMob(M_PIG,p));
}
static void tick(){
  G.prev=G.pl.pos; G.tickN++; G.worldT++; if(G.mineCD>0)G.mineCD--; if(G.invuln>0)G.invuln--;
  bool ui=G.invOpen||G.dead||G.chatOpen||G.paused;
  float F=0,S=0,ud=0; bool jump=false,sprint=false;
  if(!ui){
    if(IsKeyDown(KEY_W))F+=1; if(IsKeyDown(KEY_S))F-=1; if(IsKeyDown(KEY_D))S+=1; if(IsKeyDown(KEY_A))S-=1;
    sprint=IsKeyDown(KEY_LEFT_CONTROL)&&F>0; jump=IsKeyDown(KEY_SPACE);
    ud=(jump?1.0f:0.0f)-(IsKeyDown(KEY_LEFT_SHIFT)?1.0f:0.0f);
  }
  if(!G.dead)tickBody(G.world,G.pl,F,S,G.yaw,jump,sprint,0.1f,G.flying,ud);
  if(G.flying&&G.pl.onGround)G.flying=false;
  if(G.mode==0&&!G.flying){
    if(G.pl.inWater)G.fall=0;
    if(G.pl.onGround){if(G.fall>3.0f)hurtPlayer((int)ceilf(G.fall-3.0f),nullptr);G.fall=0;}
    else if(G.pl.pos.y<G.prev.y)G.fall+=G.prev.y-G.pl.pos.y;
    if(!G.dead){
      G.hungerT+=sprint?2:1; if(G.hungerT>=1800){G.hungerT=0;if(G.hunger>0)G.hunger--;}
      if(G.hunger>=18&&G.health<20){if(++G.regenT>=80){G.regenT=0;G.health++;}}
      else if(G.hunger==0){if(++G.regenT>=80){G.regenT=0;if(G.health>1)G.health--;}}
      else G.regenT=0;
    }
  } else G.fall=0;
  if(G.pl.pos.y<-20&&!G.dead)hurtPlayer(4,nullptr);
  if(!ui&&G.tgt.hit&&IsMouseButtonDown(MOUSE_BUTTON_LEFT))mineTick(); else if(!IsMouseButtonDown(MOUSE_BUTTON_LEFT))G.mineProg=0;
  for(size_t i=0;i<G.mobs.size();){
    Mob&m=G.mobs[i]; int d=tickMob(G.world,m,G.pl.pos,G.mode==0&&!G.dead);
    if(d>0)hurtPlayer(d,&m.b.pos);
    float dx=m.b.pos.x-G.pl.pos.x,dz=m.b.pos.z-G.pl.pos.z; float dist=sqrtf(dx*dx+dz*dz);
    bool gone=dist>80||m.b.pos.y<-10||(m.type==M_ZOMBIE&&bright()>0.7f&&dist>30);
    if(gone)G.mobs.erase(G.mobs.begin()+i); else ++i;
  }
  spawnMobs();
  if(G.mp&&(G.tickN%2==0))G.net.sendLine("POS "+std::to_string(G.pl.pos.x)+" "+std::to_string(G.pl.pos.y)+" "+std::to_string(G.pl.pos.z)+" "+std::to_string(G.yaw));
}
static bool overlapsPlayer(int x,int y,int z){
  return x<G.pl.pos.x+0.3f&&x+1>G.pl.pos.x-0.3f&&y<G.pl.pos.y+1.8f&&y+1>G.pl.pos.y&&z<G.pl.pos.z+0.3f&&z+1>G.pl.pos.z-0.3f;
}
static void eat(){
  Stack&s=G.inv[G.sel]; int v=foodValue(s.id);
  if(v>0&&G.mode==0&&G.hunger<20){G.hunger=std::min(20,G.hunger+v);if(--s.n<=0)s=Stack();playSfx(sPlace);}
}
static void interact(float dt){
  float reach=G.mode==1?6.0f:5.0f; Vector3 d=lookDir();
  G.tgt=raycast(G.world,G.eye,d,reach); G.tgtMob=-1; float best=G.tgt.hit?G.tgt.dist:reach;
  for(size_t i=0;i<G.mobs.size();i++){
    Mob&m=G.mobs[i]; float t; float hw=m.b.w/2;
    if(rayBox(G.eye,d,Vector3{m.b.pos.x-hw,m.b.pos.y,m.b.pos.z-hw},Vector3{m.b.pos.x+hw,m.b.pos.y+m.b.h,m.b.pos.z+hw},t)&&t<best){best=t;G.tgtMob=(int)i;}
  }
  if(G.tgtMob>=0)G.tgt.hit=false;
  G.placeCD-=dt;
  if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&G.tgtMob>=0){
    Mob&m=G.mobs[G.tgtMob]; int held=G.inv[G.sel].id;
    m.hp-=(int)weaponDamage(held); m.hurt=10; m.b.vel.x+=d.x*0.4f; m.b.vel.z+=d.z*0.4f; m.b.vel.y=0.36f; playSfx(sHurt);
    if(isTool(held))useTool();
    if(m.hp<=0){
      if(G.mode==0){if(m.type==M_PIG)addToInv(G.inv,irand(1,3),I_PORK);else{int k=irand(0,2);if(k)addToInv(G.inv,k,I_FLESH);}}
      G.mobs.erase(G.mobs.begin()+G.tgtMob); G.tgtMob=-1;
    }
  }
  bool use=IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)||(IsMouseButtonDown(MOUSE_BUTTON_RIGHT)&&G.placeCD<=0);
  if(use){
    G.placeCD=0.25f; Stack&s=G.inv[G.sel];
    if(G.tgt.hit&&G.world.getBlock(G.tgt.x,G.tgt.y,G.tgt.z)==B_TABLE&&IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)){G.invOpen=true;G.table=true;}
    else if(foodValue(s.id)>0)eat();
    else if(G.tgt.hit&&!s.empty()&&s.id<B_COUNT&&s.id!=B_AIR){
      int px=G.tgt.x+G.tgt.nx,py=G.tgt.y+G.tgt.ny,pz=G.tgt.z+G.tgt.nz;
      int cur=G.world.getBlock(px,py,pz);
      if((cur==B_AIR||cur==B_WATER)&&py>=0&&py<CH&&!(binfo(s.id).solid&&overlapsPlayer(px,py,pz))){
        placeNet(px,py,pz,s.id); playSfx(sPlace);
        if(G.mode==0){if(--s.n<=0)s=Stack();}
      }
    }
  }
}
static void handleNet(const std::string&l){
  std::istringstream ss(l); std::string c; ss>>c;
  if(c=="SET"){int x,y,z,id;ss>>x>>y>>z>>id;G.world.setBlock(x,y,z,id);}
  else if(c=="JOIN"){int id;std::string n;ss>>id>>n;if(id!=G.myId){Remote r;r.name=n;G.remotes[id]=r;}}
  else if(c=="LEAVE"){int id;ss>>id;G.remotes.erase(id);}
  else if(c=="POS"){
    int id;float x,y,z,yw;ss>>id>>x>>y>>z>>yw; auto it=G.remotes.find(id);
    if(it!=G.remotes.end()){Remote&r=it->second;Vector3 t{x,y,z};if(r.tgt.x==0&&r.tgt.y==0&&r.tgt.z==0)r.pos=t;r.tgt=t;r.yaw=yw;}
  }
  else if(c=="CHAT"){std::string rest;std::getline(ss,rest);G.chat.push_back(rest.size()?rest.substr(1):rest);G.chatShow=8;}
}
static void netPump(){
  for(auto&l:G.backlog)handleNet(l); G.backlog.clear();
  for(auto&l:G.net.poll())handleNet(l);
  if(!G.net.isOpen())leaveToMenu("Disconnected from server");
}
static void startConnect(){
  std::string err;
  if(!G.net.connectTo(G.pendingAddr,err)){G.msg=err;G.scr=S_SERVERS;return;}
  G.net.sendLine("HELLO "+G.playerName);
  double t0=GetTime(); bool ok=false; WorldMeta m; m.name="server";
  while(GetTime()-t0<5.0&&!ok&&G.net.isOpen()){
    for(auto&l:G.net.poll()){
      std::istringstream ss(l); std::string c; ss>>c;
      if(!ok&&c=="WELCOME"){int id;uint32_t seed;int mode;ss>>id>>seed>>mode;G.myId=id;m.seed=seed;m.mode=mode;ok=true;}
      else G.backlog.push_back(l);
    }
    WaitTime(0.02);
  }
  if(!ok){G.net.disconnect();G.msg="Server did not respond";G.scr=S_SERVERS;return;}
  std::vector<std::string> bl=G.backlog; startWorld(m,false,true); G.backlog=bl;
}

// ---------- rendering ----------
static Color tint(Color c,bool h){return h?rgba(255,c.g/2,c.b/2):c;}
static void box(float x,float y,float z,float w,float h,float l,Color c,bool hurt){
  DrawCube(Vector3{x,y,z},w,h,l,tint(c,hurt)); DrawCubeWires(Vector3{x,y,z},w,h,l,rgba(0,0,0,110));
}
static void drawHuman(Vector3 p,float yaw,Color skin,Color shirt,Color pants,bool hurt){
  rlPushMatrix(); rlTranslatef(p.x,p.y,p.z); rlRotatef(yaw*RAD2DEG,0,1,0);
  box(-0.15f,0.4f,0,0.25f,0.8f,0.25f,pants,hurt); box(0.15f,0.4f,0,0.25f,0.8f,0.25f,pants,hurt);
  box(0,1.05f,0,0.6f,0.7f,0.3f,shirt,hurt); box(0,1.65f,0,0.45f,0.45f,0.45f,skin,hurt);
  box(-0.42f,1.1f,0,0.2f,0.7f,0.2f,skin,hurt); box(0.42f,1.1f,0,0.2f,0.7f,0.2f,skin,hurt);
  rlPopMatrix();
}
static void drawMob(const Mob&m,float a){
  Vector3 p=lerp3(m.prev,m.b.pos,a); bool h=m.hurt>0;
  if(m.type==M_ZOMBIE){drawHuman(p,m.yaw,rgba(80,140,70),rgba(0,170,170),rgba(50,60,150),h);return;}
  rlPushMatrix(); rlTranslatef(p.x,p.y,p.z); rlRotatef(m.yaw*RAD2DEG,0,1,0);
  Color pink=rgba(240,160,170);
  box(0,0.55f,0,0.6f,0.5f,0.9f,pink,h); box(0,0.7f,0.6f,0.45f,0.45f,0.4f,rgba(245,175,185),h); box(0,0.62f,0.82f,0.22f,0.16f,0.1f,rgba(225,130,140),h);
  for(int i=0;i<4;i++)box((i%2?0.2f:-0.2f),0.15f,(i<2?0.3f:-0.3f),0.18f,0.3f,0.18f,rgba(215,140,150),h);
  rlPopMatrix();
}
static void draw3D(float alpha){
  float b=bright(); Color sky=rgba((int)(10+125*b),(int)(12+178*b),(int)(30+225*b));
  ClearBackground(sky);
  G.cam.position=G.eye; Vector3 d=lookDir(); G.cam.target=Vector3{G.eye.x+d.x,G.eye.y+d.y,G.eye.z+d.z};
  G.cam.up=Vector3{0,1,0}; G.cam.fovy=G.fov; G.cam.projection=CAMERA_PERSPECTIVE;
  int pcx=((int)floorf(G.pl.pos.x))>>4,pcz=((int)floorf(G.pl.pos.z))>>4;
  BeginMode3D(G.cam); rlDisableBackfaceCulling();
  int c=(int)(70+185*b); G.world.mat.maps[MATERIAL_MAP_DIFFUSE].color=rgba(c,c,c);
  G.world.draw(pcx,pcz,G.rd);
  for(auto&m:G.mobs)drawMob(m,alpha);
  for(auto&kv:G.remotes){Remote&r=kv.second;r.pos=lerp3(r.pos,r.tgt,0.25f);drawHuman(r.pos,r.yaw,rgba(220,175,140),rgba(60,120,200),rgba(60,60,110),false);}
  if(G.tgt.hit){
    Vector3 cp{G.tgt.x+0.5f,G.tgt.y+0.5f,G.tgt.z+0.5f};
    DrawCubeWires(cp,1.003f,1.003f,1.003f,BLACK);
    if(G.mineProg>0)DrawCube(cp,1.004f,1.004f,1.004f,rgba(0,0,0,(int)(G.mineProg*150)));
  }
  EndMode3D();
}
static void drawHUD(){
  float cx=LW()/2,cy=LH()/2;
  DrawRectangle((int)cx-5,(int)cy,11,1,rgba(255,255,255,220)); DrawRectangle((int)cx,(int)cy-5,1,11,rgba(255,255,255,220));
  float x0=cx-91,y0=LH()-22;
  for(int i=0;i<9;i++){
    Rectangle r=rc(x0+i*20,y0,20,20); DrawRectangleRec(r,rgba(0,0,0,130)); DrawRectangleLinesEx(r,1,rgba(90,90,90));
    drawStack(G.inv[i],x0+i*20+1,y0+1);
  }
  DrawRectangleLinesEx(rc(x0+G.sel*20-1,y0-1,22,22),2,WHITE);
  if(!G.inv[G.sel].empty()){std::string n=itemName(G.inv[G.sel].id);ctxt(n,cx,y0-26-(G.mode==0?10:0),8);}
  if(G.mode==0){
    for(int i=0;i<10;i++){
      float hx=x0+i*8,hy=y0-10; DrawRectangle((int)hx,(int)hy,7,7,rgba(50,0,0));
      if(G.health>=2*(i+1))DrawRectangle((int)hx,(int)hy,7,7,rgba(220,30,30)); else if(G.health==2*i+1)DrawRectangle((int)hx,(int)hy,4,7,rgba(220,30,30));
      float ux=x0+182-8-i*8; DrawRectangle((int)ux,(int)hy,7,7,rgba(40,25,0));
      if(G.hunger>=2*(i+1))DrawRectangle((int)ux,(int)hy,7,7,rgba(200,130,40)); else if(G.hunger==2*i+1)DrawRectangle((int)ux+3,(int)hy,4,7,rgba(200,130,40));
    }
  }
  if(G.debug){
    txt("FPS "+std::to_string(GetFPS())+"  XYZ "+std::to_string((int)G.pl.pos.x)+" "+std::to_string((int)G.pl.pos.y)+" "+std::to_string((int)G.pl.pos.z),4,4,8);
    txt(std::string("chunks ")+std::to_string(G.world.chunks.size())+"  mobs "+std::to_string(G.mobs.size())+(G.flying?"  FLYING":""),4,14,8);
  }
  if(G.mp){
    int shown=0; for(int i=(int)G.chat.size()-1;i>=0&&shown<6;i--,shown++) if(G.chatShow>0||G.chatOpen)txt(G.chat[i],4,LH()-50-shown*10,8);
    if(G.chatOpen)txt("> "+G.chatBuf+(((int)(GetTime()*2)%2)?"_":""),4,LH()-38,8);
  }
}
static void drawNames(){
  if(!G.mp)return;
  Vector3 f=lookDir();
  for(auto&kv:G.remotes){
    Vector3 p{kv.second.pos.x,kv.second.pos.y+2.1f,kv.second.pos.z};
    Vector3 v{p.x-G.eye.x,p.y-G.eye.y,p.z-G.eye.z};
    if(v.x*f.x+v.y*f.y+v.z*f.z<=0)continue;
    Vector2 sp=GetWorldToScreen(p,G.cam); ctxt(kv.second.name,sp.x/UIS,sp.y/UIS-5,8);
  }
}

static void playUpdate(float dt){
  if(G.mp)netPump(); if(G.scr!=S_PLAY)return;
  if(!G.dead){
    if(IsKeyPressed(KEY_ESCAPE)){if(G.invOpen)closeInv();else if(G.chatOpen)G.chatOpen=false;else G.paused=!G.paused;}
    else if(!G.paused&&!G.chatOpen&&IsKeyPressed(KEY_E)){if(G.invOpen)closeInv();else G.invOpen=true;}
    else if(G.mp&&!G.paused&&!G.invOpen&&!G.chatOpen&&IsKeyPressed(KEY_T)){G.chatOpen=true;G.chatBuf.clear();while(GetCharPressed()>0){}}
  }
  bool anyUI=G.invOpen||G.paused||G.dead||G.chatOpen;
  if(anyUI!=G.cursorFree){G.cursorFree=anyUI;if(anyUI)EnableCursor();else{DisableCursor();GetMouseDelta();}}
  if(G.chatOpen){
    int ch; while((ch=GetCharPressed())>0)if(ch>=32&&ch<127&&G.chatBuf.size()<100)G.chatBuf+=(char)ch;
    if((IsKeyPressed(KEY_BACKSPACE)||IsKeyPressedRepeat(KEY_BACKSPACE))&&!G.chatBuf.empty())G.chatBuf.pop_back();
    if(IsKeyPressed(KEY_ENTER)){if(!G.chatBuf.empty())G.net.sendLine("CHAT "+G.chatBuf);G.chatOpen=false;}
  }
  if(!anyUI){
    Vector2 md=GetMouseDelta(); G.yaw-=md.x*0.12f*DEG2RAD; G.pitch-=md.y*0.12f*DEG2RAD;
    G.pitch=std::clamp(G.pitch,-1.55f,1.55f);
    float w=GetMouseWheelMove(); if(w>0)G.sel=(G.sel+8)%9; else if(w<0)G.sel=(G.sel+1)%9;
    for(int i=0;i<9;i++)if(IsKeyPressed(KEY_ONE+i))G.sel=i;
    if(IsKeyPressed(KEY_F)&&G.mode==1){G.flying=!G.flying;G.pl.vel.y=0;}
    if(IsKeyPressed(KEY_F3))G.debug=!G.debug;
  }
  if(!G.paused||G.mp){G.acc+=std::min(dt,0.25f);while(G.acc>=0.05f){tick();G.acc-=0.05f;}}
  float alpha=G.acc/0.05f; Vector3 p=lerp3(G.prev,G.pl.pos,alpha); G.eye=Vector3{p.x,p.y+1.62f,p.z};
  G.world.update(((int)floorf(G.pl.pos.x))>>4,((int)floorf(G.pl.pos.z))>>4,G.rd,2,3);
  if(!anyUI)interact(dt); else G.tgt.hit=false;
  G.chatShow-=dt;
}

// ---------- screens ----------
static void screenMenu(){
  dirtBG(); float cx=LW()/2;
  ctxt("BLOCKCRAFT",cx,28,30,rgba(255,255,255)); ctxt("PE",cx+tw("BLOCKCRAFT",30)/2-6,58,10,rgba(255,255,85));
  ctxt("original voxel sandbox - not affiliated with Mojang",cx,76,8,rgba(200,200,200));
  float y=100; Rectangle r=rc(cx-90,y,180,22);
  if(button(r,"Play")){refreshWorlds();G.scr=S_WORLDS;}
  r.y+=28; if(button(r,"Servers")){loadServers();G.msg="";G.scr=S_SERVERS;}
  r.y+=28; if(button(r,"Settings"))G.scr=S_SETTINGS;
  r.y+=28; if(button(r,"Quit"))G.quit=true;
  if(!G.msg.empty())ctxt(G.msg,cx,LH()-20,8,rgba(255,120,120));
}
static void screenWorlds(){
  dirtBG(); float cx=LW()/2; ctxt("Select World",cx,14,12);
  float y=36; int idx=0;
  for(size_t i=0;i<G.worlds.size()&&idx<7;i++,idx++){
    WorldMeta&m=G.worlds[i];
    if(button(rc(cx-110,y,200,22),m.name+(m.mode==1?"  [Creative]":"  [Survival]"))){startWorld(m,true,false);return;}
    if(button(rc(cx+94,y,16,22),"x")){fs::remove(savePath(m.name));refreshWorlds();return;}
    y+=26;
  }
  if(G.worlds.empty())ctxt("No saved worlds yet",cx,60,10,rgba(200,200,200));
  if(button(rc(cx-90,LH()-56,180,22),"Create New World")){G.fSeed.clear();G.createMode=0;G.scr=S_CREATE;}
  if(button(rc(cx-90,LH()-30,180,22),"Back"))G.scr=S_MENU;
}
static void screenCreate(){
  dirtBG(); float cx=LW()/2; ctxt("Create New World",cx,14,12);
  txt("World name",cx-90,40,8); field(1,rc(cx-90,50,180,20),G.fName,"New World",20);
  txt("Seed (blank = random)",cx-90,78,8); field(2,rc(cx-90,88,180,20),G.fSeed,"random",16);
  if(button(rc(cx-90,118,180,22),G.createMode==0?"Game Mode: Survival":"Game Mode: Creative"))G.createMode^=1;
  bool ok=!G.fName.empty();
  if(button(rc(cx-90,150,180,22),"Create World",ok)){
    WorldMeta m; m.name=G.fName; for(auto&ch:m.name)if(!isalnum((unsigned char)ch)&&ch!=' '&&ch!='_'&&ch!='-')ch='_';
    if(G.fSeed.empty())m.seed=(uint32_t)rng()(); else{bool num=true;for(char ch:G.fSeed)if(!isdigit((unsigned char)ch))num=false;m.seed=num?(uint32_t)std::stoul(G.fSeed):(uint32_t)std::hash<std::string>{}(G.fSeed);}
    m.mode=G.createMode; startWorld(m,false,false); return;
  }
  if(button(rc(cx-90,176,180,22),"Back"))G.scr=S_WORLDS;
}
static void screenServers(){
  dirtBG(); float cx=LW()/2; ctxt("Servers",cx,14,12);
  float y=36; for(size_t i=0;i<G.servers.size()&&i<6;i++){
    ServerEntry&s=G.servers[i]; bool sel=(int)i==G.selServer;
    if(sel)DrawRectangleRec(rc(cx-112,y-2,224,26),rgba(255,255,255,70));
    if(button(rc(cx-110,y,220,22),s.name+"  "+s.addr))G.selServer=(int)i;
    y+=26;
  }
  if(!G.msg.empty())ctxt(G.msg,cx,LH()-100,8,rgba(255,120,120));
  bool has=G.selServer>=0&&G.selServer<(int)G.servers.size();
  if(button(rc(cx-110,LH()-84,106,22),"Join Server",has)){G.pendingAddr=G.servers[G.selServer].addr;G.connectDelay=0;G.scr=S_CONNECTING;}
  if(button(rc(cx+4,LH()-84,106,22),"Add / Direct")){G.fSrvName.clear();G.fSrvAddr.clear();G.scr=S_ADDSRV;}
  if(button(rc(cx-110,LH()-58,106,22),"Delete",has)){G.servers.erase(G.servers.begin()+G.selServer);G.selServer=-1;saveServers();}
  if(button(rc(cx+4,LH()-58,106,22),"Back"))G.scr=S_MENU;
}
static void screenAddServer(){
  dirtBG(); float cx=LW()/2; ctxt("Add Server / Direct Connect",cx,14,12);
  txt("Server name",cx-90,40,8); field(3,rc(cx-90,50,180,20),G.fSrvName,"My Server",20);
  txt("IP address (ip:port, default port 25566)",cx-90,78,8); field(4,rc(cx-90,88,180,20),G.fSrvAddr,"127.0.0.1:25566",40);
  bool ok=!G.fSrvAddr.empty();
  if(button(rc(cx-90,120,180,22),"Join Now",ok)){G.pendingAddr=G.fSrvAddr;G.connectDelay=0;G.scr=S_CONNECTING;}
  if(button(rc(cx-90,146,180,22),"Save Server",ok)){G.servers.push_back({G.fSrvName.empty()?G.fSrvAddr:G.fSrvName,G.fSrvAddr});saveServers();G.scr=S_SERVERS;}
  if(button(rc(cx-90,172,180,22),"Back"))G.scr=S_SERVERS;
}
static void screenSettings(){
  dirtBG(); float cx=LW()/2; ctxt("Settings",cx,14,12);
  if(button(rc(cx-90,40,180,22),"Render distance: "+std::to_string(G.rd)))G.rd=G.rd>=8?3:G.rd+1;
  if(button(rc(cx-90,68,180,22),"FOV: "+std::to_string((int)G.fov)))G.fov=G.fov>=100?60:G.fov+10;
  if(button(rc(cx-90,96,180,22),G.music?"Music: ON":"Music: OFF"))G.music=!G.music;
  txt("Player name (multiplayer)",cx-90,126,8); field(9,rc(cx-90,136,180,20),G.playerName,"Player",16);
  if(button(rc(cx-90,170,180,22),"Back")){if(G.playerName.empty())G.playerName="Player";for(auto&ch:G.playerName)if(ch==' ')ch='_';G.scr=S_MENU;}
}
static void screenLoading(){
  int pcx=((int)floorf(G.pl.pos.x))>>4,pcz=((int)floorf(G.pl.pos.z))>>4;
  G.world.update(pcx,pcz,3,8,6);
  int ready=0; for(int dz=-3;dz<=3;dz++)for(int dx=-3;dx<=3;dx++){Chunk*c=G.world.get(pcx+dx,pcz+dz);if(c&&c->hasMesh)ready++;}
  dirtBG(); float cx=LW()/2;
  ctxt(G.mp?"Joining server...":"Generating world...",cx,LH()/2-20,12);
  DrawRectangle((int)cx-60,(int)LH()/2,120,8,rgba(0,0,0,200)); DrawRectangle((int)cx-60,(int)LH()/2,(int)(120.0f*ready/49.0f),8,rgba(90,200,90));
  ctxt("Tip: craft a pickaxe before digging into stone.",cx,LH()/2+20,8,rgba(210,210,210));
  if(ready>=49||++G.loadFrames>1500){
    while(boxHit(G.world,G.pl)&&G.pl.pos.y<CH-3)G.pl.pos.y+=1;
    G.prev=G.pl.pos; G.scr=S_PLAY; G.cursorFree=true;
  }
}
static void screenPause(){
  DrawRectangle(0,0,(int)LW(),(int)LH(),rgba(0,0,0,150)); float cx=LW()/2; ctxt("Game Menu",cx,LH()/2-60,12);
  if(button(rc(cx-90,LH()/2-35,180,22),"Resume"))G.paused=false;
  if(button(rc(cx-90,LH()/2-7,180,22),G.mp?"Disconnect":"Save & Quit to Title"))leaveToMenu();
}
static void screenDead(){
  DrawRectangle(0,0,(int)LW(),(int)LH(),rgba(120,0,0,150)); float cx=LW()/2; ctxt("You died!",cx,LH()/2-50,20);
  if(button(rc(cx-90,LH()/2-10,180,22),"Respawn")){
    G.pl.pos=G.spawn;G.pl.vel=Vector3{0,0,0};G.prev=G.spawn;G.health=20;G.hunger=20;G.fall=0;G.dead=false;G.mobs.clear();
    while(boxHit(G.world,G.pl)&&G.pl.pos.y<CH-3)G.pl.pos.y+=1;
  }
  if(button(rc(cx-90,LH()/2+16,180,22),"Title Screen"))leaveToMenu();
}

int main(){
  SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_VSYNC_HINT);
  InitWindow(960,540,"BlockCraft PE"); SetExitKey(0); SetTargetFPS(120);
  InitAudioDevice(); makeAtlas(); G.world.initGfx(G.atlas); synth.init();
  if(IsAudioDeviceReady()){sDig=makeSfx(220,90,0.12f,0.6f);sPlace=makeSfx(300,180,0.08f,0.4f);sHurt=makeSfx(180,60,0.2f,0.5f);sfxOk=true;}
  loadServers();
  while(!WindowShouldClose()&&!G.quit){
    float dt=GetFrameTime(); UIS=std::max(1.0f,GetScreenHeight()/300.0f);
    Vector2 mp=GetMousePosition(); M=Vector2{mp.x/UIS,mp.y/UIS};
    clk=IsMouseButtonPressed(MOUSE_BUTTON_LEFT); rclk=IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    if(G.activeField<0)while(GetCharPressed()>0){}
    synth.update(G.music);
    if(G.scr==S_PLAY)playUpdate(dt);
    BeginDrawing();
    Camera2D c2{}; c2.zoom=UIS;
    if(G.scr==S_PLAY){
      draw3D(G.acc/0.05f);
      BeginMode2D(c2); drawHUD(); drawNames();
      if(G.invOpen)invUI(); if(G.paused)screenPause(); if(G.dead)screenDead();
      EndMode2D();
    } else {
      ClearBackground(BLACK); BeginMode2D(c2);
      switch(G.scr){
        case S_MENU:screenMenu();break; case S_WORLDS:screenWorlds();break; case S_CREATE:screenCreate();break;
        case S_SERVERS:screenServers();break; case S_ADDSRV:screenAddServer();break; case S_SETTINGS:screenSettings();break;
        case S_LOADING:screenLoading();break;
        case S_CONNECTING:dirtBG();ctxt("Connecting to "+G.pendingAddr+"...",LW()/2,LH()/2-5,10);break;
        default:break;
      }
      EndMode2D();
    }
    EndDrawing();
    if(G.scr==S_CONNECTING&&++G.connectDelay>=2)startConnect();
  }
  if(G.scr==S_PLAY)saveWorld();
  G.net.disconnect(); G.world.clear(); CloseAudioDevice(); CloseWindow();
  return 0;
}
