#pragma once

#include <string>
#include <cctype>

#include "../../config/LocationConfig.hpp"
#include "../../config/ServerConfig.hpp"
#include "../Request.hpp"

class HandlerUtils
{
	public:
		static std::string uriPath(const Request &request);
		static bool hasUnsafeSegment(const std::string &path);
		static bool buildResourcePath(const Request &request, const ServerConfig &server, const LocationConfig &location, std::string &filePath);
		static bool buildUploadPath(const Request &request, const LocationConfig &location, std::string &filePath);
		static bool closeConnection(const Request &request);
		static std::string contentType(const std::string &path);
};
