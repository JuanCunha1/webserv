#pragma once

#include <string>
#include <vector>
#include <cctype>
#include "config/StructsConfig.hpp"

class ConfigParser {
	private:
		enum DirectiveType {
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

		enum LocationDirectiveType {
			LOC_ALLOWED_METHODS,
			LOC_AUTOINDEX,
			LOC_UPLOAD_PATH,
			LOC_ROOT,
			LOC_INDEX,
			LOC_UNKNOWN
		};

		std::vector<std::string>	_tokens;
		std::vector<ServerConfig>	_servers;

		void tokenize(const std::string &content);
		void parseServers();

		void expect(const std::string &token, size_t &pos);
		std::string readValue(size_t &pos);
		void parseServer(size_t &pos, ServerConfig &server);

		DirectiveType getDirectiveType(const std::string &directive) const;

		LocationDirectiveType getLocationDirectiveType(
		const std::string &directive) const;

		void parseLocation(size_t &pos, ServerConfig &server);
		void validateServers() const;
	public:
		ConfigParser();

		void parseFile(const std::string &filename);

		const std::vector<ServerConfig> &getServers() const;
	};

/*#include "config/ConfigServer.hpp"

#include <vector>
#include <string>
#include <fstream> // File Stream: to read and write files on disk

#include <iostream>
#include <cstdlib> // std::atoi
#include <cctype> // std::isdigit
#include <sys/stat.h>

class ConfigParser
{
	public:
		// ConfigParser.cpp
		ConfigParser();
		ConfigParser( const ConfigParser& original );
		ConfigParser& operator=( const ConfigParser& rhs );
		~ConfigParser();
		const std::vector<ConfigServer>& getServers() const;
		void parseFile(const std::string& file);

	private:
		std::vector<std::string>	_tokens;
		size_t						_tokenIndex;
		std::vector<ConfigServer>	_servers;

		// ConfigParserPrevious.cpp
		bool    _openFile(std::ifstream& file, const std::string& filename);
		void    _readFileAndTokenize(std::ifstream& file);
		void    _notReadingCommentsInConfigFile(std::string& line);
		bool    _bracesChecker();

		// ConfigParserTokensBlocks.cpp
		bool    _parseTokens();
		bool    _parseServerBlock(ConfigServer& server);
		bool    _parseLocationBlock(ConfigLocation& location);
		bool    _duplicateErrorMessage(const std::string& directive);

		// ConfigParserTypes.cpp
		bool    _parseSingleString(std::string& target, const std::string& key);
		bool    _parseNumber(int& target, const std::string& key);
		bool    _parseVector(std::vector<std::string>& target, const std::string& key);
		bool    _parseReturn(std::vector<ConfigRedirections>& target);
		bool    _parseClientMaxBodySize(unsigned long& target, const std::string& valStr);
		
		// ConfigParserValidations.cpp
		bool    _validateSemantic();
		bool    _validatePathTraversal();
		bool    _validateRedirections();
		bool    _validateDirectoriesExist();
		void    _warnPrivilegedPorts();


		// ConfigParserPost.cpp
		bool    _postProcessConfiguration();
		void    _inheritServerToLocation();
		void    _normalizePaths();
		void    _normalizeCGIExtensions();
};*/