#include "parser.hpp"
#include "http_encap.hpp"
#include "compress.hpp"

#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <sys/epoll.h>

namespace fs = std::filesystem;

constexpr int MAX_EVENTS = 10;
constexpr int MAX_CLIENTS = 10;
constexpr int PORT = 4221;

std::string DIR_PATH = fs::current_path().string();

bool init_server(int &socket_fd, sockaddr_in &server_addr){
  // Create socket
  socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if(socket_fd < 0){
   std::cerr << "Failed to create server socket\n";
   return false;
  }
  
  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0){
    std::cerr << "setsockopt failed\n";
    return false;
  }
  
  // Bind socket to address and port
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);
  if(bind(socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0){
    std::cerr << "Failed to bind to port " << PORT << std::endl;
    close(socket_fd);
    return false;
  }

  return true;
}

void send_answer(int& c_fd, HTTP_Encap& answer){
  std::string temp = answer.to_string();
  ssize_t total = 0;
  ssize_t len = temp.size();

  while(total < len){
    ssize_t sent = send(c_fd, temp.data() + total, len - total, 0);
    if(sent <= 0) break;
    total += sent;
  }
}

void handle_client(int client_fd){
  bool use_compress = false;
  bool end_conn = false;
  std::string request;
  char buf[4096]; 

  while(request.find("\r\n\r\n") == std::string::npos){
    ssize_t request_size = read(client_fd, buf, 4096);
    if(request_size <= 0) break;

    request.append(buf, request_size);
  }

  if(!request.empty())
    std::cout << request << std::endl;

  std::vector<std::string> request_line = rl_parser(request);
  std::vector<std::string> headers = headers_parser(request);
  std::string body_content = body_parser(request);

  if(request_line.size() < 2)
    return;

  const std::string method = request_line[0];
  const std::string target = request_line[1];
  HTTP_Encap answer;

  for(const auto& h : headers){
    std::string lower_h = to_lower(h);
    
    if(lower_h.find("connection") != std::string::npos && lower_h.find("close") != std::string::npos){
      end_conn = true;
      answer.add_header("Connection", "close");
    }
    
    if(lower_h.find("accept-encoding") != std::string::npos){
      std::string compress_types = h.substr(h.find(":") + 2);
      if(compress_types.find("gzip") != std::string::npos){
        use_compress = true;
        answer.add_header("Content-Encoding", "gzip"); // change static compress 
        break;
      }
    }
  }

  if(target == "/"){
    answer.set_msg_type(Resp_Type::_200);
    send_answer(client_fd, answer);    
  }
  else if(target.find("/files") != std::string::npos){
    const std::string fname = target.substr(target.find("files/") + 6);

    if(fname.empty()){
      answer.set_msg_type(Resp_Type::_404);
      send_answer(client_fd, answer);
      return;
    }

    std::string file_path = DIR_PATH + "/" + fname;

    if(method == "GET"){
      if(fs::exists(file_path)){
        auto entry = fs::directory_entry(file_path);
        fs::perms p = fs::status(entry).permissions();
        if(fs::is_regular_file(entry) && 
          (p & fs::perms::owner_read) != fs::perms::none ||
          (p & fs::perms::group_read) != fs::perms::none ||
          (p & fs::perms::others_read) != fs::perms::none)
        {
          std::ifstream file(entry.path());
          if(!file.is_open()){
            std::cerr << "Unable to read " << entry.path().filename() << " in " << entry.path() << std::endl;
          }
          
          std::string content, line;
          while(std::getline(file, line)){
            content += line + "\n";
          }

          answer.set_msg_type(Resp_Type::_200);
          answer.add_header("Content-Type", "application/octet-stream");
          
          if(use_compress){
            std::vector<unsigned char> compressed = gzip_compress(content.c_str(), content.size());
            
            answer.add_header("Content-Length", std::to_string(compressed.size()));
            answer.add_body(std::string((char*) compressed.data(), compressed.size()));
          } else{
            answer.add_header("Content-Length", std::to_string(entry.file_size()));
            answer.add_body(content);
          }

          file.close();
          send_answer(client_fd, answer);
        }
      }
      else{
        answer.set_msg_type(Resp_Type::_404);
        send_answer(client_fd, answer);
      }
    }
    else if(method == "POST"){
      int file_fd = open(file_path.c_str(),
                      O_WRONLY | O_CREAT | O_TRUNC,
                      0644);
      if(file_fd < 0){
        std::cerr << "POST: Unable to create file\n";
        answer.set_msg_type(Resp_Type::_404);
      }

      ssize_t wtotal = 0;
      ssize_t to_write = body_content.size();
      const char* data = body_content.data();

      while(wtotal < to_write){
        ssize_t written = write(file_fd,
                                data + wtotal,
                                to_write - wtotal);
        
        if(written < 0){
          if(errno == EINTR)
            continue;
          
          perror("write");
          close(file_fd);
          std::cerr << "POST: Unable to wite on file\n";
          if(errno == EACCES)
            answer.set_msg_type(Resp_Type::_403);
          else if(errno == ENOENT)
            answer.set_msg_type(Resp_Type::_404);
          else
            answer.set_msg_type(Resp_Type::_500);

          break;
        }

        wtotal += written;
      }

      if(wtotal == to_write){
        close(file_fd);
        answer.set_msg_type(Resp_Type::_201);
      }
      
      send_answer(client_fd, answer);
    }
  }
  else if(target.find("/echo") != std::string::npos){
    const std::string content = target.substr(target.find_last_of('/') + 1);
    answer.set_msg_type(Resp_Type::_200);
    answer.add_header("Content-Type", "text/plain");
    
    if(use_compress){
      std::vector<unsigned char> compressed = gzip_compress(content.c_str(), content.size());
      
      answer.add_header("Content-Length", std::to_string(compressed.size()));
      answer.add_body(std::string((char*) compressed.data(), compressed.size()));
    } else{
      answer.add_header("Content-Length", std::to_string(content.size()));
      answer.add_body(content);
    }

    send_answer(client_fd, answer);
  }
  else if(target.find("/user-agent") != std::string::npos){
    std::string content;
    for(const auto& h : headers){
      std::string lower = to_lower(h);
      if(lower.find("user-agent:") != std::string::npos)
        content = h.substr(h.find(": ") + 2, h.size() - 2);
    }

    if(!content.empty()){
      answer.set_msg_type(Resp_Type::_200);
      answer.add_header("Content-Type", "text/plain");
      
      if(use_compress){
        std::vector<unsigned char> compressed = gzip_compress(content.c_str(), content.size());
        
        answer.add_header("Content-Length", std::to_string(compressed.size()));
        answer.add_body(std::string((char*) compressed.data(), compressed.size()));
      } else{
        answer.add_header("Content-Length", std::to_string(content.size()));
        answer.add_body(content);
      }

      send_answer(client_fd, answer);
    } else{
      answer.set_msg_type(Resp_Type::_404);
      send_answer(client_fd, answer);
    }
  }
  else{
    answer.set_msg_type(Resp_Type::_404);
    send_answer(client_fd, answer);
  }
  
  if(end_conn)
    close(client_fd);
}

bool unlock_fd(int fd){
  int flags = fcntl(fd, F_GETFL, 0);
  if(flags == -1)
    return false;
  
  int status = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if(status == -1)
      return false;

  return true;
}

void accept_conn(int server_fd, int epoll_fd, epoll_event event){
  while(true){
    // Accept new client
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    if(client_fd == -1){
      if(errno == EAGAIN || errno == EWOULDBLOCK)
        break;
      else
        perror("accept");
    }

    unlock_fd(client_fd);

    // Add client to epoll instance
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = client_fd;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1){
      std::cerr << "Failed to add client to epoll instance\n";
      close(client_fd);
    }
  }
}

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  int server_fd, epoll_fd;
  struct sockaddr_in server_addr;
  struct epoll_event event, events[MAX_EVENTS];
  
  // Catch initialization arguments
  for(int i=1; i < argc; i++){
    std::string arg = argv[i];
    if(arg == "--directory" || arg == "-d"){
      if(i + 1 < argc){
        DIR_PATH = argv[++i];

        if(!fs::exists(DIR_PATH)){
          std::cerr << "directory: " << DIR_PATH << " does not exists\n";
          return 1;
        }

        if(!fs::is_directory(DIR_PATH)){
          std::cerr << "directory: " << DIR_PATH << " is not a directory\n";
          return 1;
        }
      }
    }
  }

  if(!init_server(server_fd, server_addr))
    return 1;
  
  // Listen for incommig connections
  if (listen(server_fd, MAX_CLIENTS) != 0) {
    std::cerr << "listen failed\n";
    close(server_fd);
    return 1;
  }

  // Create epoll instance
  epoll_fd = epoll_create1(0);
  if(epoll_fd == -1){
    std::cerr << "Failed to create an epoll instance\n";
    close(epoll_fd);
    close(server_fd);
    return 1;
  }

  //Add server socket to epoll
  event.events = EPOLLIN | EPOLLET;
  event.data.fd = server_fd;
  if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1){
    std::cerr << "Failed to add server socket to epoll instance\n";
    close(epoll_fd);
    close(server_fd);
    return 1;
  }

  std::cout << "Server started. Listenig on port " << PORT << std::endl;

  while(true){
    int nEvents = epoll_wait(epoll_fd, events, MAX_EVENTS, 20000);
    if(nEvents == -1){
      std::cerr << "Failed to wait for events\n";
      break;
    }
    if(nEvents == 0){
      std::cout << "Timeout reached. Exiting...\n";
      break;
    }

    for(int i=0; i < nEvents; i++){
      if(events[i].events & EPOLLERR ||
         events[i].events & EPOLLHUP ||
         !(events[i].events & EPOLLIN))
      {
        close(events[i].data.fd);
      }
      else if(events[i].data.fd == server_fd){
        accept_conn(server_fd, epoll_fd, event);
      }
      else{
        int client_fd = events[i].data.fd;
        unlock_fd(client_fd);
        handle_client(client_fd);
      }
    }
  }
  
  close(epoll_fd);
  close(server_fd);
  std::cout << "Server closed" << std::endl;
  return 0;
}