#include "parser.hpp"
#include <charconv>

#include <iostream>

std::string to_lower(std::string str){
  for(auto& ch : str)
    ch = std::tolower(static_cast<unsigned char>(ch));

  return str;
}

std::vector<std::string> rl_parser(const std::string& raw_request){
    std::vector<std::string> request_line;

    size_t n = raw_request.find("\r\n");
    if(n == std::string::npos) return {};

    size_t offset = 0;
    std::string token;
    while(offset < n){
        char ch = raw_request[offset];
        
        if(ch == ' '){
            request_line.push_back(token);
            token.clear();
        } else
            token += ch;

        offset++;
    }

    return request_line;
}

std::vector<std::string> headers_parser(const std::string& raw_request){
    std::vector<std::string> headers;
    std::vector<std::string> request;

    size_t n, offset = 0;
    while(offset < raw_request.size()){
        n = raw_request.find("\r\n", offset);
        if(n == std::string::npos) break;
    
        request.push_back(raw_request.substr(offset, n - offset));
        offset = n + 2;
    }

    for(auto& entry : request){
        if(entry.find(": ") != std::string::npos){
            headers.push_back(entry);
        }
    }

    return headers;
}

std::string body_parser(const std::string& raw_request){
    std::string header;
    for(const auto& h : headers_parser(raw_request)){
        if(to_lower(h).find("content-length") != std::string::npos){
            header = h;
            break;
        }
    }

    if(header.empty())
        return "";

    size_t pos = raw_request.find("\r\n\r\n");
    if(pos == std::string::npos)
        return "";

    if(pos + 4 >= raw_request.size())
        return "";

    pos += 4;
    size_t hpos = header.find(":");
    if(hpos == std::string::npos || hpos >= header.size() - 2)
        return "";

    header = header.substr(hpos + 2);
    int body_size;

    auto [ptr, ec] = std::from_chars(
        header.data(),
        header.data() + header.size() - 2,
        body_size
    );

    if(ec != std::errc())
        return "";

    return raw_request.substr(pos, body_size);
}