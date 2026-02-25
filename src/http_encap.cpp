#include "http_encap.hpp"

const std::string HTTP_VER = "HTTP/1.1 ";

std::string msg(Resp_Type t){
    switch(t){
        case Resp_Type::_200: return "200 OK\r\n";
        case Resp_Type::_201: return "201 CREATED\r\n";
        case Resp_Type::_404: return "404 BAD REQUEST\r\n";
        default: return "UNKNOW\r\n";
    }
}

HTTP_Encap::HTTP_Encap(){
    msg_type = "UNKNOW\r\n";
    body = "";
}

void HTTP_Encap::set_msg_type(Resp_Type t){
    HTTP_Encap::msg_type = msg(t);
}

void HTTP_Encap::add_header(std::string key, std::string content){
    header_vec.push_back(key + ": " + content + "\r\n");
}

void HTTP_Encap::add_body(std::string content){
    body = content;
}

std::string HTTP_Encap::to_string(){
    std::string header;
    if(!header_vec.empty()){
        for(const auto& h : header_vec)
            header += h;
    }
    
    header += "\r\n";

    return HTTP_VER + msg_type + header + body;
}