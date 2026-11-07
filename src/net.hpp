#pragma once
// Tiny line-based TCP client (platform headers are kept out of raylib's translation unit).
#include <string>
#include <vector>
class NetClient{
public:
  bool connectTo(const std::string&addr,std::string&err); // "host:port" (default port 25566)
  void sendLine(const std::string&s);
  std::vector<std::string> poll();
  void disconnect();
  bool isOpen()const{return fd>=0;}
private:
  long long fd=-1; std::string buf;
};
