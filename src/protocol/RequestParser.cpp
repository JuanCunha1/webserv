#include "RequestParser.hpp"

#include <cstdlib>
#include <cctype>
#include <climits>

RequestParser::RequestParser(size_t maxBodySize)
	: _state(REQUEST_LINE), _buffer(), _request(), _maxBodySize(maxBodySize), _chunkSize(0)
{
}

RequestParser::~RequestParser()
{
}

std::string RequestParser::trim(const std::string &str)
{
	size_t first = 0;
	size_t last = str.size();

	while (first < last &&
		   (str[first] == ' ' || str[first] == '\t'))
		++first;

	while (last > first &&
		   (str[last - 1] == ' ' || str[last - 1] == '\t'))
		--last;

	return str.substr(first, last - first);
}

std::string RequestParser::toLower(const std::string &str)
{
	std::string result = str;

	for (size_t i = 0; i < result.size(); ++i)
		result[i] = static_cast<char>(
			std::tolower(static_cast<unsigned char>(result[i]))
		);

	return result;
}

bool RequestParser::fail()
{
	_state = ERROR;
	return false;
}

RequestParser::State RequestParser::getState() const
{
	return _state;
}

const HttpRequest &RequestParser::getRequest() const
{
	return _request;
}

bool RequestParser::isComplete() const
{
	return _state == COMPLETE;
}

void RequestParser::reset()
{
	_state = REQUEST_LINE;
	_buffer.clear();
	_request = HttpRequest();
	_chunkSize = 0;
}

bool RequestParser::parseRequestLine(const std::string &line)
{
	size_t firstSpace = line.find(' ');

	if (firstSpace == std::string::npos || firstSpace == 0)
		return fail();

	size_t secondSpace = line.find(' ', firstSpace + 1);

	if (secondSpace == std::string::npos ||
		secondSpace == firstSpace + 1 ||
		secondSpace + 1 >= line.size())
		return fail();

	if (line.find(' ', secondSpace + 1) != std::string::npos)
		return fail();

	std::string method = line.substr(0, firstSpace);
	std::string uri = line.substr(
		firstSpace + 1,
		secondSpace - firstSpace - 1
	);
	std::string version = line.substr(secondSpace + 1);

	if (version != "HTTP/1.1" && version != "HTTP/1.0")
		return fail();

	for (size_t i = 0; i < method.size(); ++i)
	{
		unsigned char c = static_cast<unsigned char>(method[i]);

		if (!std::isalpha(c) && !std::isdigit(c) &&
			std::string("!#$%&'*+-.^_`|~").find(c)
				== std::string::npos)
			return fail();
	}

	if (uri.find_first_of(" \r\n\t") != std::string::npos)
		return fail();

	_request.setMethod(method);
	_request.setUri(uri);
	_request.setVersion(version);

	return true;
}