#pragma once

#include <string>
#include <vector>

enum class Resp_Type{
    _200,
    _201,
    _403,
    _404,
    _500
};

class HTTP_Encap{
private:
    std::string msg_type;
    std::vector<std::string> header_vec;
    std::string body;

public:
    explicit HTTP_Encap();

    void set_msg_type(Resp_Type t);

    void add_header(std::string key, std::string content);

    void add_body(std::string content);

    std::string to_string();
};