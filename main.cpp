#include <iostream>
#include <fstream>
#include "htpp.hpp"

void Root(Request* req) {
  std::string key = req->HttpHeaders.Get("Key").c_str();
  std::string name = std::to_string(atoi(key.c_str()) % 2) + ".txt";
  
  std::ifstream fin(name);
  std::string value;
  
  while (!fin.eof()) {
    std::string delim = ":";
    getline(fin, value);
    std::string str = value.substr(0, value.find(delim));
    value.erase(0, value.find(delim) + delim.length());
    
    if (atoi(str.c_str()) == atoi(key.c_str())) {
      req->Respond(200, str + ":" + value);
      break;
    }
  }
  
  req->Respond(404, "Error: No such key!");
}

void Save(Request* req) {
  std::string val = req->HttpHeaders.Get("Key").c_str(), key = val;
  std::string delim = "/";
  
  key = val.substr(0, val.find(delim));
  val.erase(0, val.find(delim) + delim.length());

  std::string name = std::to_string(atoi(key.c_str()) % 2) + ".txt";
  std::ofstream fout(name, std::ios_base::app);
  fout << key << ':' << val << '\n';
  
  req->Respond(201, "OK");
}

void Post(Request* req) {
  std::cout << req->Body << "\n";

  req->Respond(200, "");
}

int main() {
  Server server = Server();

  server.SetCGIDir("/cgi", "dir");

  server.Get("/", Root);

  server.Get("/save", Save);

  server.Post("/post", Post);

  server.Start(8080);

  while (server.Running);
}