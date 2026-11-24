#pragma once

#include "Response.hpp"

class ResponseBuilder
{
    public:
        static Response build(const std::string &version ,int status, const std::string &reason, const std::string &body, const std::string &contentType, bool closeConnection);
        static Response ok(const std::string &version ,const std::string &body, const std::string &contentType);
        static Response error(const std::string &version ,int status, const std::string &reason);

};