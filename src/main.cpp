#include <iostream>
#include <thread>
#include <cstdlib>
#include <unordered_map>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>
#include <sstream>
#include <cctype>
#include <stdio.h>
#include <tuple>
#include <chrono>
#include <cmath>
#include <queue>

std::unordered_map<std::string, std::tuple<std::string,int,std::chrono::time_point<std::chrono::high_resolution_clock>>> global_dictionary;
std::unordered_map<std::string, std::vector<std::string>> RList;
std::unordered_map<std::string, std::chrono::time_point<std::chrono::high_resolution_clock>> BLPOP;



std::vector<std::string> parser(const std::string& input_string, std::string delimiter="\r\n"){
  
  std::vector<std::string> current;
  std::string lower = input_string;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
   
  int start = 0;
  int current_length = delimiter.length();

  

    while(lower.find(delimiter,start) != std::string::npos){
        int result = lower.find(delimiter,start);
        current.push_back(lower.substr(start,result-start));
        start = result+current_length;
    } 
    
    current.push_back(lower.substr(start));
 

    return current;
}

std::string make_bulk(const std::string& input_1, const std::string& input_2){
  std::string result = input_1+"\r\n"+input_2+"\r\n";
  return result;
}


void handler(int client_fd){
   char buffer[1024];
  while(1){
     ssize_t n = recv(client_fd,buffer,sizeof(buffer)-1,0);
  if(n <= 0){
    break;
  }
  buffer[n] = '\0';

  std::vector<std::string> RESP_array = parser(buffer);
  
  if(RESP_array.size() > 2 && RESP_array[2] == "echo"){
    std::string temp;
    temp = make_bulk(RESP_array[3],RESP_array[4]);
    
    send(client_fd, temp.c_str(), strlen(temp.c_str()), 0);
  }

  else if(RESP_array.size() > 6 && RESP_array[2]=="set"){
    global_dictionary[RESP_array[4]]=std::tuple<std::string,int,std::chrono::time_point<std::chrono::high_resolution_clock>>(RESP_array[6],0,std::chrono::high_resolution_clock::now());

    if(RESP_array.size()>=10 && RESP_array[8]=="ex"){
      std::get<1>(global_dictionary[RESP_array[4]])= std::stoi(RESP_array[10]) * 1000;
    }
    else if(RESP_array.size()>=10 && RESP_array[8]=="px"){
       std::get<1>(global_dictionary[RESP_array[4]])= std::stoi(RESP_array[10]);
    }


      send(client_fd, "+OK\r\n",strlen("+OK\r\n"),0);
    
  }

  else if(RESP_array.size() > 2 && RESP_array[2]=="get"){

    auto t = std::chrono::high_resolution_clock::now();
    auto ret_val = global_dictionary.find(RESP_array[4]);
    if(ret_val == global_dictionary.end()){
      send(client_fd,"$-1\r\n",strlen("$-1\r\n"),0);
    }
    else{

      auto t2 = std::get<2>(global_dictionary[RESP_array[4]])
          + std::chrono::milliseconds(std::get<1>(global_dictionary[RESP_array[4]]));


      if(t2 >= t || !std::get<1>(global_dictionary[RESP_array[4]])){
      std::string temp = "$" + make_bulk(std::to_string((std::get<0>(global_dictionary[RESP_array[4]])).length()),std::get<0>(global_dictionary[RESP_array[4]]));
      send(client_fd,temp.c_str(),temp.length(),0);
      }
      else{
        send(client_fd,"$-1\r\n",strlen("$-1\r\n"),0);
      }
    }
  }
  else if(RESP_array.size()>2 && RESP_array[2]=="rpush"){
   
    for(int i=6;i<RESP_array.size()-1;i+=2){
        RList[RESP_array[4]].push_back(RESP_array[i]);
    }
    

    std::string temp = ":" + std::to_string(RList[RESP_array[4]].size())+"\r\n";
    send(client_fd,temp.c_str(),temp.length(),0);
   
   
  }
  else if (RESP_array.size() > 2 && RESP_array[2] == "lrange") {
    
    std::string key_name = RESP_array[4];
    int starting = std::stoi(RESP_array[6]);
    int ending   = std::stoi(RESP_array[8]);

    int sz = static_cast<int>(RList[key_name].size());
    if(sz <=0){
      send(client_fd, "*0\r\n", strlen("*0\r\n"), 0);
      continue;
    }
    
    if(starting<=-1){
      starting = ((sz + starting)<0) ? 0: sz+starting;
     
    }
    if(ending <=-1){
      ending = ((sz + ending)<0) ? 0 : sz+ending;
    
    }

    auto it = RList.find(key_name);
    if(it == RList.end()){
      send(client_fd, "*0\r\n", strlen("*0\r\n"), 0);

    }

    else if (starting >= sz || starting > ending) {

        send(client_fd, "*0\r\n", strlen("*0\r\n"), 0);
       
    }
    else{

   
    int last = std::min(ending,sz-1);
    int count = last - starting + 1;

    std::string response = "*" + std::to_string(count) + "\r\n";

    for (int i = starting; i <=last; i++) {
        const std::string& val = RList[key_name][i];
        response += "$" + std::to_string(val.length()) + "\r\n" + val + "\r\n";
    }

    send(client_fd, response.c_str(), response.size(), 0);
  }
}
else if(RESP_array.size() > 2 && RESP_array[2] == "lpush"){

    for(int i=6;i<RESP_array.size()-1;i+=2){
        RList[RESP_array[4]].insert(RList[RESP_array[4]].begin(),RESP_array[i]);
    }
    

    std::string temp = ":" + std::to_string(RList[RESP_array[4]].size())+"\r\n";
    send(client_fd,temp.c_str(),temp.length(),0);
   
}
else if(RESP_array.size() > 2 && RESP_array[2] == "llen"){


  std::string temp = ":" + std::to_string(RList[RESP_array[4]].size())+"\r\n";
    send(client_fd,temp.c_str(),temp.length(),0);
}
else if(RESP_array.size() > 2 && RESP_array[2] == "lpop"){

  if((RESP_array.size()>=8)){

    int temp_x = std::stoi(RESP_array[6]);
    std::string current = "*"+RESP_array[6]+"\r\n";


    for(int i=0; i<temp_x;i++){
      auto it = RList[RESP_array[4]].front();
      current+="$" + std::to_string(it.length())+"\r\n"+it+"\r\n";
      RList[RESP_array[4]].erase(RList[RESP_array[4]].begin());
    }

    send(client_fd,current.c_str(),current.length(),0);
  }

  else if(!RList[RESP_array[4]].empty()){
     auto it = RList[RESP_array[4]].front();
    std::string example = "$" + std::to_string(it.length()) + "\r\n" +it+"\r\n";
    RList[RESP_array[4]].erase(RList[RESP_array[4]].begin());
    send(client_fd, example.c_str(),example.length(),0);
  
  }

  else{
  send(client_fd,"$-1\r\n",strlen("$-1\r\n"),0);
  }

}
  else if(RESP_array.size() > 2 && RESP_array[2] == "blpop"){
    auto x = std::chrono::high_resolution_clock::time_point::min();
  if(RESP_array[6]!="0"){
    x = std::chrono::high_resolution_clock::now() + std::chrono::seconds(std::stoi(RESP_array[6]));
  }

BLPOP[RESP_array[4]] = x ;     

  } 

  else{
    send(client_fd,"+PONG\r\n",7,0);
  }
  

  
  n--;
  }
  
}

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 6379\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  std::cout << "Waiting for a client to connect...\n";

  
  

 while(1){
  int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
  if(client_fd <= 0){
    continue;
  }
  std::thread(handler,client_fd).detach();
 }
  
  close(server_fd);

  return 0;
}
