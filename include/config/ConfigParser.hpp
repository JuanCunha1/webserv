#pragma once
#include "config/ConfigTokenizer.hpp"
#include "config/ConfigValidator.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "config/ServerConfig.hpp"

class ConfigParser
{
private:
	enum DirectiveType
	{
		DIR_LISTEN,
		DIR_HOST,
		DIR_SERVER_NAME,
		DIR_ROOT,
		DIR_INDEX,
		DIR_LOCATION,
		DIR_CLIENT_MAX_BODY_SIZE,
		DIR_ERROR_PAGE,
		DIR_UNKNOWN
	};

	enum LocationDirectiveType
	{
		LOC_ALLOWED_METHODS,
		LOC_AUTOINDEX,
		LOC_UPLOAD_PATH,
		LOC_ROOT,
		LOC_INDEX,
		LOC_UNKNOWN
	};

	struct ServerDirectivesState
	{
		bool hasListen;
		bool hasHost;
		bool hasServerName;
		bool hasRoot;
		bool hasIndex;
		bool hasMaxBodySize;

		ServerDirectivesState()
			: hasListen(false),
			hasHost(false),
			hasServerName(false),
			hasRoot(false),
			hasIndex(false),
			hasMaxBodySize(false)
		{
		}
	};

	struct LocationDirectivesState
	{
		bool hasAllowedMethods;
		bool hasAutoindex;
		bool hasUploadPath;
		bool hasRoot;
		bool hasIndex;

		LocationDirectivesState()
			: hasAllowedMethods(false),
			hasAutoindex(false),
			hasUploadPath(false),
			hasRoot(false),
			hasIndex(false)
		{
		}
	};

	std::vector<std::string> _tokens;
	std::vector<ServerConfig> _servers;

	void parseServers();
	void parseServer(std::size_t &pos, ServerConfig &server);
	void parseLocation(std::size_t &pos, ServerConfig &server);

	void parseListen(std::size_t &pos, ServerConfig &server);
	void parseHost(std::size_t &pos, ServerConfig &server);
	void parseServerName(std::size_t &pos, ServerConfig &server);
	void parseRoot(std::size_t &pos, ServerConfig &server);
	void parseIndex(std::size_t &pos, ServerConfig &server);
	void parseClientMaxBodySize(std::size_t &pos, ServerConfig &server);
	void parseErrorPage(std::size_t &pos, ServerConfig &server);

	void expect(const std::string &token, std::size_t &pos);
	std::string readValue(std::size_t &pos);
	std::size_t parseUnsigned(
		const std::string &value,
		const std::string &directive) const;

	DirectiveType getDirectiveType(const std::string &directive) const;
	LocationDirectiveType getLocationDirectiveType(
		const std::string &directive) const;

public:
	ConfigParser();

	void parseFile(const std::string &filename);
	const std::vector<ServerConfig> &getServers() const;
};
