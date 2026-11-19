#pragma once

#include <string>
#include <vector>

class ConfigTokenizer
{
public:
    static std::vector<std::string> tokenize(const std::string &content);
};
