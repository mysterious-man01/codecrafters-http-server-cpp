#pragma once
#include <string>
#include <vector>
using namespace std;

vector<string> rl_parser(const string& raw_request);

vector<string> headers_parser(const string& raw_request);