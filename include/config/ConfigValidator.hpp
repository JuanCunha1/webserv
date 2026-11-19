#pragma once

#include <vector>

#include "config/ServerConfig.hpp"

class ConfigValidator
{
public:
    static void validate(const std::vector<ServerConfig> &servers);
};
