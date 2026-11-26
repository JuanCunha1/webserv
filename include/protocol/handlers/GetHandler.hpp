#pragma once

#include "../../config/LocationConfig.hpp"
#include "../../config/ServerConfig.hpp"
#include "../Request.hpp"
#include "../Response.hpp"

class GetHandler
{
    public:
        static Response handle(const Request &request, const ServerConfig &server, const LocationConfig &location);
};
