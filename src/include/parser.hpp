#pragma once
#include <string>
#include <vector>

std::string to_lower(std::string str);

std::vector<std::string> rl_parser(const std::string& raw_request);

std::vector<std::string> headers_parser(const std::string& raw_request);

std::string body_parser(const std::string& raw_request);