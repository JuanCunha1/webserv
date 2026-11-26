#pragma once

#include "../../config/LocationConfig.hpp"
#include "../Request.hpp"
#include "../Response.hpp"

class DeleteHandler
{
    public:
        static Response handle(const Request &request, const LocationConfig &location );
};

