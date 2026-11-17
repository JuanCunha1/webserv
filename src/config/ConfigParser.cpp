#include "config/ConfigParser.hpp"

#include <fstream>
#include <sstream>
#include <iostream>


ConfigParser::ConfigParser()
{
}

void ConfigParser::parseServers()
{
	size_t pos = 0;
	_servers.clear();
	while (pos < _tokens.size())
	{
		if (_tokens[pos] != "server")
			throw std::runtime_error(
				"ConfigParser: expected 'server', got '"
				+ _tokens[pos] + "'"
			);
		++pos;
		expect("{", pos);

		ServerConfig server;
		parseServer(pos, server);

		_servers.push_back(server);
	}

	if (_servers.empty())
		throw std::runtime_error(
			"ConfigParser: no server defined"
		);
}

ConfigParser::DirectiveType
ConfigParser::getDirectiveType(const std::string &directive) const
{
	if (directive == "listen")
		return DIR_LISTEN;
	if (directive == "host")
		return DIR_HOST;
	if (directive == "server_name")
		return DIR_SERVER_NAME;
	if (directive == "root")
		return DIR_ROOT;
	if (directive == "index")
		return DIR_INDEX;
	if (directive == "location")
		return DIR_LOCATION;
	if (directive == "client_max_body_size")
		return DIR_CLIENT_MAX_BODY_SIZE;
	if (directive == "error_page")
		return DIR_ERROR_PAGE;
	return DIR_UNKNOWN;
}

void ConfigParser::parseServer(size_t &pos, ServerConfig &server)
{
	while(pos < _tokens.size() && _tokens[pos] != "}")
	{
		std::string directive = _tokens[pos++];
		switch (getDirectiveType(directive))
		{
			case DIR_LISTEN:
			{
				std::string value = readValue(pos);
				int port = 0;

				if (value.empty())
					throw std::runtime_error("Empty port");

				for (size_t i = 0; i < value.size(); ++i)
				{
					if (!std::isdigit(
							static_cast<unsigned char>(value[i])))
						throw std::runtime_error("Invalid port");

					port = port * 10 + (value[i] - '0');

					if (port > 65535)
						throw std::runtime_error("Port out of range");
				}

				if (port < 1)
					throw std::runtime_error("Invalid port");

				server.port = port;
				expect(";", pos);
				break;
			}
			case DIR_HOST:
				server.host = readValue(pos);
				expect(";", pos);
				break;
			case DIR_SERVER_NAME:
				server.serverName = readValue(pos);
				expect(";", pos);
				break;
			case DIR_ROOT:
				server.root = readValue(pos);
				expect(";", pos);
				break;
			case DIR_LOCATION:
				parseLocation(pos, server);
				break;
			case DIR_INDEX:
				server.index = readValue(pos);
				expect(";", pos);
			 	break;
			case DIR_CLIENT_MAX_BODY_SIZE:
			{
				std::string value = readValue(pos);

				if (value.empty())
					throw std::runtime_error(
						"client_max_body_size cannot be empty");

				size_t maxSize = 0;

				for (size_t i = 0; i < value.size(); ++i)
				{
					if (value[i] < '0' || value[i] > '9')
						throw std::runtime_error(
							"Invalid client_max_body_size");

					size_t digit = value[i] - '0';

					if (maxSize > (static_cast<size_t>(-1) - digit) / 10)
						throw std::runtime_error(
							"client_max_body_size overflow");

					maxSize = maxSize * 10 + digit;
				}

				server.maxBodySize = maxSize;
				expect(";", pos);
				break;
			}
			case DIR_ERROR_PAGE:
			{
				std::string codeValue = readValue(pos);
				std::string page = readValue(pos);

				int code = 0;

				if (codeValue.empty())
					throw std::runtime_error("Missing error_page status code");

				for (size_t i = 0; i < codeValue.size(); ++i)
				{
					if (codeValue[i] < '0' || codeValue[i] > '9')
						throw std::runtime_error("Invalid error_page status code");

					code = code * 10 + (codeValue[i] - '0');
				}

				if (code < 400 || code > 599)
					throw std::runtime_error("Invalid HTTP error status code");

				if (server.errorPages.find(code) != server.errorPages.end())
					throw std::runtime_error("Duplicate error_page status code");

				server.errorPages[code] = page;
				expect(";", pos);
				break;
			}
			default:
				throw std::runtime_error(
					"ConfigParser: unknown directive '" + directive + "'"
				);
		}
	}
	expect("}", pos);
}

void ConfigParser::expect(const std::string &token, size_t &pos)
{
	if (pos >= _tokens.size() || _tokens[pos] != token)
		throw std::runtime_error(
			"ConfigParser: expected '" + token + "'"
		);

	++pos;
}

std::string ConfigParser::readValue(size_t &pos)
{
	if (pos >= _tokens.size())
		throw std::runtime_error(
			"ConfigParser: expected a value"
		);

	if (_tokens[pos] == "{" ||
		_tokens[pos] == "}" ||
		_tokens[pos] == ";")
		throw std::runtime_error(
			"ConfigParser: invalid directive value"
		);

	return _tokens[pos++];
}

void ConfigParser::parseFile(const std::string &filename)
{
	std::ifstream file(filename.c_str());
	if (!file.is_open())
		throw std::runtime_error("Error: Could not open configuration file: " + filename);

	std::stringstream buffer;
	buffer << file.rdbuf();
	if (file.bad())
		throw std::runtime_error(
			"ConfigParser: error reading config file: " + filename
		);
	std::string content = buffer.str();
	file.close();
	_servers.clear();
	tokenize(content);

	parseServers();

	validateServers();
}

const std::vector<ServerConfig> &ConfigParser::getServers() const
{
	return _servers;
}

void ConfigParser::tokenize(const std::string &content)
{
	std::string token;
	_tokens.clear();
	
	for (size_t i = 0; i < content.size(); ++i)
	{
		char c = content[i];
		if (c == '#')
		{
			while (i < content.size() && content[i] != '\n')
				++i;
		}
		else if (std::isspace(static_cast<unsigned char>(c)))
		{
			if (!token.empty())
			{
				_tokens.push_back(token);
				token.clear();
			}
		}
		else if (c == '{' || c == '}' || c == ';')
		{
			if (!token.empty())
			{
				_tokens.push_back(token);
				token.clear();
			}
			_tokens.push_back(std::string(1, c));
		}
		else
		{
			token += c;
		}
	}
	if (!token.empty())
		_tokens.push_back(token);
}

ConfigParser::LocationDirectiveType
ConfigParser::getLocationDirectiveType(
	const std::string &directive) const
{
	if (directive == "allowed_methods")
		return LOC_ALLOWED_METHODS;
	if (directive == "autoindex")
		return LOC_AUTOINDEX;
	if (directive == "upload_path")
		return LOC_UPLOAD_PATH;
	if (directive == "root")
		return LOC_ROOT;
	if (directive == "index")
		return LOC_INDEX;

	return LOC_UNKNOWN;
}

void ConfigParser::parseLocation(
	size_t &pos,
	ServerConfig &server)
{
	LocationConfig location;

	location.path = readValue(pos);
	expect("{", pos);

	while (pos < _tokens.size() && _tokens[pos] != "}")
	{
		std::string directive = readValue(pos);

		switch (getLocationDirectiveType(directive))
		{
			case LOC_ALLOWED_METHODS:
			{
				while (pos < _tokens.size() && _tokens[pos] != ";")
				{
					std::string method = readValue(pos);

					if (method != "GET"
						&& method != "POST"
						&& method != "DELETE")
					{
						throw std::runtime_error(
							"Invalid HTTP method: " + method);
					}
					location.allowedMethods.push_back(method);
				}
				expect(";", pos);
				if (location.allowedMethods.empty())
					throw std::runtime_error(
						"allowed_methods cannot be empty");
				break;
			}
			case LOC_AUTOINDEX:
			{
				std::string value = readValue(pos);

				if (value == "on")
					location.autoindex = true;
				else if (value == "off")
					location.autoindex = false;
				else
					throw std::runtime_error(
						"autoindex must be on or off");
				expect(";", pos);
				break;
			}
			case LOC_UPLOAD_PATH:
				location.uploadPath = readValue(pos);
				expect(";", pos);
				break;
			case LOC_ROOT:
				location.root = readValue(pos);
				expect(";", pos);
				break;
			case LOC_INDEX:
				location.index = readValue(pos);
				expect(";", pos);
				break;
			default:
				throw std::runtime_error(
					"Unknown location directive: " + directive);
		}
	}
	expect("}", pos);
	server.locations.push_back(location);
}

void ConfigParser::validateServers() const
{
	if (_servers.empty())
		throw std::runtime_error(
			"ConfigParser: no server defined");
	for (size_t i = 0; i < _servers.size(); ++i)
	{
		const ServerConfig &server = _servers[i];

		if (server.port < 1 || server.port > 65535)
			throw std::runtime_error(
				"ConfigParser: invalid server port");

		if (server.host.empty())
			throw std::runtime_error(
				"ConfigParser: server host cannot be empty");
		
		if (server.root.empty())
			throw std::runtime_error(
				"ConfigParser: server root cannot be empty");
		for (size_t j = 0; j < server.locations.size(); ++j)
		{
			const LocationConfig &location = server.locations[j];

			if (location.path.empty())
				throw std::runtime_error(
					"ConfigParser: location path cannot be empty");

			if (location.allowedMethods.empty())
				throw std::runtime_error(
					"ConfigParser: location must define allowed_methods");
		}
	}
}