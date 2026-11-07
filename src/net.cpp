#include "net.hpp"
#include <cstring>
#include <cerrno>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define BADSOCK INVALID_SOCKET
#define CLOSESOCK ::closesocket
static bool wouldBlock(){return WSAGetLastError()==WSAEWOULDBLOCK;}
static bool inProgress(){return WSAGetLastError()==WSAEWOULDBLOCK;}
static void nonblock(sock_t s){u_long m=1;ioctlsocket(s,FIONBIO,&m);}
static void initNet(){static bool d=false;if(!d){WSADATA w;WSAStartup(MAKEWORD(2,2),&w);d=true;}}
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <csignal>
typedef int sock_t;
#define BADSOCK (-1)
#define CLOSESOCK ::close
static bool wouldBlock(){return errno==EAGAIN||errno==EWOULDBLOCK;}
static bool inProgress(){return errno==EINPROGRESS;}
static void nonblock(sock_t s){fcntl(s,F_SETFL,fcntl(s,F_GETFL,0)|O_NONBLOCK);}
static void initNet(){signal(SIGPIPE,SIG_IGN);}
#endif

bool NetClient::connectTo(const std::string&addr,std::string&err){
  initNet(); disconnect();
  std::string host=addr,port="25566"; size_t p=addr.rfind(':');
  if(p!=std::string::npos){host=addr.substr(0,p);port=addr.substr(p+1);}
  if(host.empty())host="127.0.0.1";
  addrinfo hints{}; hints.ai_family=AF_UNSPEC; hints.ai_socktype=SOCK_STREAM; addrinfo*res=nullptr;
  if(getaddrinfo(host.c_str(),port.c_str(),&hints,&res)!=0||!res){err="Unknown host";return false;}
  sock_t s=BADSOCK; bool ok=false;
  for(addrinfo*a=res;a&&!ok;a=a->ai_next){
    s=socket(a->ai_family,a->ai_socktype,a->ai_protocol); if(s==BADSOCK)continue;
    nonblock(s);
    int r=connect(s,a->ai_addr,(int)a->ai_addrlen);
    if(r==0)ok=true;
    else if(inProgress()){
      fd_set ws; FD_ZERO(&ws); FD_SET(s,&ws); timeval tv; tv.tv_sec=4; tv.tv_usec=0;
      if(select((int)s+1,nullptr,&ws,nullptr,&tv)>0){int e=0;socklen_t l=sizeof(e);getsockopt(s,SOL_SOCKET,SO_ERROR,(char*)&e,&l);ok=(e==0);}
    }
    if(!ok){CLOSESOCK(s);s=BADSOCK;}
  }
  freeaddrinfo(res);
  if(!ok){err="Could not connect to server";return false;}
  int one=1; setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(const char*)&one,sizeof(one));
  fd=(long long)s; buf.clear(); return true;
}
void NetClient::sendLine(const std::string&s){
  if(fd<0)return; std::string d=s+"\n"; size_t off=0; int tries=0;
  while(off<d.size()&&tries<500){
    int r=(int)send((sock_t)fd,d.data()+off,(int)(d.size()-off),0);
    if(r>0){off+=(size_t)r;continue;}
    if(r<0&&wouldBlock()){tries++;continue;}
    disconnect();return;
  }
}
std::vector<std::string> NetClient::poll(){
  std::vector<std::string> out; if(fd<0)return out;
  char tmp[4096];
  for(;;){
    int r=(int)recv((sock_t)fd,tmp,sizeof(tmp),0);
    if(r>0){buf.append(tmp,(size_t)r);continue;}
    if(r==0){disconnect();break;}
    if(wouldBlock())break;
    disconnect();break;
  }
  size_t p;
  while((p=buf.find('\n'))!=std::string::npos){
    std::string l=buf.substr(0,p); buf.erase(0,p+1);
    if(!l.empty()&&l.back()=='\r')l.pop_back();
    if(!l.empty())out.push_back(l);
  }
  return out;
}
void NetClient::disconnect(){if(fd>=0){CLOSESOCK((sock_t)fd);fd=-1;}}
