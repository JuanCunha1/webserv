#pragma once

#include "../../config/LocationConfig.hpp"
#include "../Request.hpp"
#include "../Response.hpp"

class PostHandler
{
    public:
        static Response handle(const Request &request, const LocationConfig &location);
};
