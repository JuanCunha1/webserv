#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "config/LocationConfig.hpp"

struct ServerConfig
{
    int port;
    std::string host;
    std::string serverName;
    std::string root;
    std::string index;
    std::size_t maxBodySize;
    std::map<int, std::string> errorPages;
    std::vector<LocationConfig> locations;

    ServerConfig()
        : port(0),
          maxBodySize(1048576)
    {
    }
};
