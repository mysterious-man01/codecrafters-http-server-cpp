#include "parser.hpp"


vector<string> rl_parser(const string& raw_request){
    vector<string> request_line;

    size_t n = raw_request.find("\r\n");
    if(n == string::npos) return {};

    size_t offset = 0;
    string token;
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

vector<string> headers_parser(const string& raw_request){
    vector<string> headers;
    vector<string> request;

    size_t n, offset = 0;
    while(offset < raw_request.size()){
        n = raw_request.find("\r\n", offset);
        if(n == string::npos) break;
    
        request.push_back(raw_request.substr(offset, n - offset));
        offset = n + 1;
    }

    for(auto& entry : request){
        if(entry.find(": ") != string::npos){
            headers.push_back(entry);
        }
    }

    return headers;
}