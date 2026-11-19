#pragma once

#include <string>
#include <vector>

struct LocationConfig
{
    std::string path;
    std::vector<std::string> allowedMethods;
    bool autoindex;
    std::string uploadPath;
    std::string root;
    std::string index;

    LocationConfig()
        : autoindex(false)
    {
    }
};
