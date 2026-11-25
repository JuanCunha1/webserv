#pragma once

#include <unistd.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <ctime>
#include <iostream>
#include "protocol/RequestParser.hpp"

#define DEBUG 1

#include <iostream>

#define LOG(message) \
    do { \
        if (DEBUG) \
            std::cout << message << std::endl; \
    } while (0)

#define DEBUG_ERROR(message) \
    do { \
        if (DEBUG) \
            std::cerr << message << std::endl; \
    } while (0)
	
class Client
{
	public:
		enum IOResult
		{
			IO_SUCCESS,
			IO_CLOSED,
			IO_ERROR,
			IO_WOULD_BLOCK
		};

	private:
		int         _fd;
		int         _serverPort;
		size_t      _maxBodySize;

		std::string _responseBuffer;

		std::time_t _lastActivity;

		bool        _shouldClose;
		bool        _readClosed;
		bool        _requestHandled;

		RequestParser _parser;

		Client();
		Client(const Client &other);
		Client &operator=(const Client &other);

	public:
		Client(int fd, int serverPort, size_t maxBodySize);
		~Client();

		int getFd() const;
		int getServerPort() const;

		IOResult receive();
		void setResponse(const std::string &response);
		bool hasDataToSend() const;

		IOResult sendData();

		const std::string &getResponseBuffer() const;

		bool isTimedOut(std::time_t now, int timeout) const;

		bool isReadClosed() const;
		void setReadClosed(bool value);

		bool hasCompleteRequest() const;
		bool hasRequestError() const;

		const Request &getRequest() const;
		const std::string &getRequestError() const;

		void resetRequest();
		bool beginRequestHandling();

		void setShouldClose(bool value);
		bool shouldClose() const;

		void refreshActivityTimer();
};