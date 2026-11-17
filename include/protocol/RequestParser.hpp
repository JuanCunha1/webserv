#pragma once

#include <string>
#include <map>
#include "HttpRequest.hpp"

class RequestParser
{
public:
	enum State
	{
		REQUEST_LINE,
		HEADERS,
		BODY_CONTENT_LENGTH,
		CHUNK_SIZE,
		CHUNK_DATA,
		CHUNK_TRAILER,
		COMPLETE,
		ERROR
	};

private:
	State		_state;
	std::string	_buffer;
	HttpRequest	_request;
	size_t		_maxBodySize;
	size_t		_chunkSize;

	bool parseRequestLine(const std::string &line);
	bool parseHeaderLine(const std::string &line);
	bool processHeaders();
	bool processChunkSize(const std::string &line);
	bool fail();

	static std::string trim(const std::string &str);
	static std::string toLower(const std::string &str);

public:
	explicit RequestParser(size_t maxBodySize);
	~RequestParser();

	bool parse(const std::string &data);

	State getState() const;
	const HttpRequest &getRequest() const;
	bool isComplete() const;
	void reset();
};
