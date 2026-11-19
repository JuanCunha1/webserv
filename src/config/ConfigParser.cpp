#include "config/ConfigParser.hpp"

ConfigParser::ConfigParser()
{
}

void ConfigParser::parseFile(const std::string &filename)
{
	std::ifstream file(filename.c_str());
	if (!file.is_open())
		throw std::runtime_error(
			"ConfigParser: cannot open configuration file: " + filename);

	std::stringstream buffer;
	buffer << file.rdbuf();

	if (file.bad())
		throw std::runtime_error(
			"ConfigParser: error reading configuration file: " + filename);

	_tokens = ConfigTokenizer::tokenize(buffer.str());
	_servers.clear();

	parseServers();
	ConfigValidator::validate(_servers);
}

const std::vector<ServerConfig> &ConfigParser::getServers() const
{
	return _servers;
}

void ConfigParser::parseServers()
{
	std::size_t pos = 0;

	while (pos < _tokens.size())
	{
		if (_tokens[pos] != "server")
			throw std::runtime_error(
				"ConfigParser: expected 'server', got '" + _tokens[pos] + "'");

		++pos;
		expect("{", pos);

		ServerConfig server;
		parseServer(pos, server);
		_servers.push_back(server);
	}

	if (_servers.empty())
		throw std::runtime_error("ConfigParser: no server defined");
}

void ConfigParser::parseServer(std::size_t &pos, ServerConfig &server)
{
	ServerDirectivesState state;

	while (pos < _tokens.size() && _tokens[pos] != "}")
	{
		const std::string directive = _tokens[pos++];

		switch (getDirectiveType(directive))
		{
			case DIR_LISTEN:
				if (state.hasListen)
					throw std::runtime_error("ConfigParser: duplicate listen directive");
				parseListen(pos, server);
				state.hasListen = true;
				break;
			case DIR_HOST:
				if (state.hasHost)
					throw std::runtime_error("ConfigParser: duplicate host directive");
				parseHost(pos, server);
				state.hasHost = true;
				break;
			case DIR_SERVER_NAME:
				if (state.hasServerName)
					throw std::runtime_error("ConfigParser: duplicate server_name directive");
				parseServerName(pos, server);
				state.hasServerName = true;
				break;
			case DIR_ROOT:
				if (state.hasRoot)
					throw std::runtime_error("ConfigParser: duplicate root directive");
				parseRoot(pos, server);
				state.hasRoot = true;
				break;
			case DIR_INDEX:
				if (state.hasIndex)
					throw std::runtime_error("ConfigParser: duplicate index directive");
				parseIndex(pos, server);
				state.hasIndex = true;
				break;
			case DIR_CLIENT_MAX_BODY_SIZE:
				if (state.hasMaxBodySize)
					throw std::runtime_error(
						"ConfigParser: duplicate client_max_body_size directive");
				parseClientMaxBodySize(pos, server);
				state.hasMaxBodySize = true;
				break;
			case DIR_ERROR_PAGE:
				parseErrorPage(pos, server);
				break;
			case DIR_LOCATION:
				parseLocation(pos, server);
				break;
			default:
				throw std::runtime_error(
					"ConfigParser: unknown server directive: " + directive);
		}
	}

	expect("}", pos);
}

void ConfigParser::parseListen(std::size_t &pos, ServerConfig &server)
{
	const std::size_t port = parseUnsigned(readValue(pos), "listen");
	if (port < 1 || port > 65535)
		throw std::runtime_error("ConfigParser: listen port must be 1..65535");

	server.port = static_cast<int>(port);
	expect(";", pos);
}

void ConfigParser::parseHost(std::size_t &pos, ServerConfig &server)
{
	server.host = readValue(pos);
	if (server.host.empty())
		throw std::runtime_error("ConfigParser: host cannot be empty");
	expect(";", pos);
}

void ConfigParser::parseServerName(std::size_t &pos, ServerConfig &server)
{
	server.serverName = readValue(pos);
	if (server.serverName.empty())
		throw std::runtime_error("ConfigParser: server_name cannot be empty");
	expect(";", pos);
}

void ConfigParser::parseRoot(std::size_t &pos, ServerConfig &server)
{
	server.root = readValue(pos);
	if (server.root.empty())
		throw std::runtime_error("ConfigParser: root cannot be empty");
	expect(";", pos);
}

void ConfigParser::parseIndex(std::size_t &pos, ServerConfig &server)
{
	server.index = readValue(pos);
	if (server.index.empty())
		throw std::runtime_error("ConfigParser: index cannot be empty");
	expect(";", pos);
}

void ConfigParser::parseClientMaxBodySize(
	std::size_t &pos, ServerConfig &server)
{
	server.maxBodySize = parseUnsigned(
		readValue(pos), "client_max_body_size");
	expect(";", pos);
}

void ConfigParser::parseErrorPage(
	std::size_t &pos, ServerConfig &server)
{
	const std::size_t code = parseUnsigned(
		readValue(pos), "error_page");
	const std::string page = readValue(pos);

	if (code < 400 || code > 599)
		throw std::runtime_error(
			"ConfigParser: error_page status must be between 400 and 599");
	if (page.empty())
		throw std::runtime_error("ConfigParser: error_page path cannot be empty");

	if (server.errorPages.find(static_cast<int>(code)) != server.errorPages.end())
		throw std::runtime_error("ConfigParser: duplicate error_page status code");

	server.errorPages[static_cast<int>(code)] = page;
	expect(";", pos);
}

void ConfigParser::parseLocation(
	std::size_t &pos, ServerConfig &server)
{
	LocationConfig location;
	location.path = readValue(pos);

	if (location.path.empty() || location.path[0] != '/')
		throw std::runtime_error(
			"ConfigParser: location path must start with '/'");

	expect("{", pos);

	LocationDirectivesState state;

	while (pos < _tokens.size() && _tokens[pos] != "}")
	{
		const std::string directive = _tokens[pos++];

		switch (getLocationDirectiveType(directive))
		{
			case LOC_ALLOWED_METHODS:
				if (state.hasAllowedMethods)
					throw std::runtime_error(
						"ConfigParser: duplicate allowed_methods directive");
				while (pos < _tokens.size() && _tokens[pos] != ";")
				{
					const std::string method = readValue(pos);
					if (method != "GET" && method != "POST" && method != "DELETE")
						throw std::runtime_error(
							"ConfigParser: invalid HTTP method: " + method);
					if (std::find(location.allowedMethods.begin(),
								  location.allowedMethods.end(), method)
						!= location.allowedMethods.end())
						throw std::runtime_error(
							"ConfigParser: duplicate HTTP method: " + method);
					location.allowedMethods.push_back(method);
				}
				expect(";", pos);
				if (location.allowedMethods.empty())
					throw std::runtime_error(
						"ConfigParser: allowed_methods cannot be empty");
				state.hasAllowedMethods = true;
				break;

			case LOC_AUTOINDEX:
			{
				if (state.hasAutoindex)
					throw std::runtime_error(
						"ConfigParser: duplicate autoindex directive");
				const std::string value = readValue(pos);
				if (value == "on")
					location.autoindex = true;
				else if (value == "off")
					location.autoindex = false;
				else
					throw std::runtime_error(
						"ConfigParser: autoindex must be on or off");
				expect(";", pos);
				state.hasAutoindex = true;
				break;
			}

			case LOC_UPLOAD_PATH:
				if (state.hasUploadPath)
					throw std::runtime_error(
						"ConfigParser: duplicate upload_path directive");
				location.uploadPath = readValue(pos);
				if (location.uploadPath.empty())
					throw std::runtime_error(
						"ConfigParser: upload_path cannot be empty");
				expect(";", pos);
				state.hasUploadPath = true;
				break;

			case LOC_ROOT:
				if (state.hasRoot)
					throw std::runtime_error(
						"ConfigParser: duplicate location root directive");
				location.root = readValue(pos);
				if (location.root.empty())
					throw std::runtime_error(
						"ConfigParser: location root cannot be empty");
				expect(";", pos);
				state.hasRoot = true;
				break;

			case LOC_INDEX:
				if (state.hasIndex)
					throw std::runtime_error(
						"ConfigParser: duplicate location index directive");
				location.index = readValue(pos);
				if (location.index.empty())
					throw std::runtime_error(
						"ConfigParser: location index cannot be empty");
				expect(";", pos);
				state.hasIndex = true;
				break;

			default:
				throw std::runtime_error(
					"ConfigParser: unknown location directive: " + directive);
		}
	}

	expect("}", pos);
	server.locations.push_back(location);
}

void ConfigParser::expect(const std::string &token, std::size_t &pos)
{
	if (pos >= _tokens.size() || _tokens[pos] != token)
	{
		const std::string received = (pos < _tokens.size())
			? "'" + _tokens[pos] + "'" : "end of file";
		throw std::runtime_error(
			"ConfigParser: expected '" + token + "', got " + received);
	}
	++pos;
}

std::string ConfigParser::readValue(std::size_t &pos)
{
	if (pos >= _tokens.size())
		throw std::runtime_error(
			"ConfigParser: expected a value, got end of file");

	if (_tokens[pos] == "{" || _tokens[pos] == "}" || _tokens[pos] == ";")
		throw std::runtime_error("ConfigParser: invalid directive value");

	return _tokens[pos++];
}

std::size_t ConfigParser::parseUnsigned(
	const std::string &value, const std::string &directive) const
{
	if (value.empty())
		throw std::runtime_error(
			"ConfigParser: " + directive + " cannot be empty");

	std::size_t result = 0;
	for (std::size_t i = 0; i < value.size(); ++i)
	{
		if (!std::isdigit(static_cast<unsigned char>(value[i])))
			throw std::runtime_error(
				"ConfigParser: invalid numeric value for " + directive);

		const std::size_t digit = static_cast<std::size_t>(value[i] - '0');
		if (result > (std::numeric_limits<std::size_t>::max() - digit) / 10)
			throw std::runtime_error(
				"ConfigParser: numeric overflow in " + directive);
		result = result * 10 + digit;
	}
	return result;
}

ConfigParser::DirectiveType ConfigParser::getDirectiveType(
	const std::string &directive) const
{
	if (directive == "listen") return DIR_LISTEN;
	if (directive == "host") return DIR_HOST;
	if (directive == "server_name") return DIR_SERVER_NAME;
	if (directive == "root") return DIR_ROOT;
	if (directive == "index") return DIR_INDEX;
	if (directive == "location") return DIR_LOCATION;
	if (directive == "client_max_body_size") return DIR_CLIENT_MAX_BODY_SIZE;
	if (directive == "error_page") return DIR_ERROR_PAGE;
	return DIR_UNKNOWN;
}

ConfigParser::LocationDirectiveType ConfigParser::getLocationDirectiveType(
	const std::string &directive) const
{
	if (directive == "allowed_methods") return LOC_ALLOWED_METHODS;
	if (directive == "autoindex") return LOC_AUTOINDEX;
	if (directive == "upload_path") return LOC_UPLOAD_PATH;
	if (directive == "root") return LOC_ROOT;
	if (directive == "index") return LOC_INDEX;
	return LOC_UNKNOWN;
}
