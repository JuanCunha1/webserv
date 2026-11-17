#pragma once

#include <unistd.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <ctime>
#include <iostream>
class Client
{
	public:
		enum IOResult
		{
			IO_SUCCESS,
			IO_CLOSED,
			IO_ERROR
		};

	private:
		int         _fd;
		int         _serverPort;
		size_t      _maxBodySize;

		std::string _requestBuffer;
		std::string _responseBuffer;

		std::time_t _lastActivity;

		bool        _shouldClose;
		bool        _readClosed;

		Client();
		Client(const Client &other);
		Client &operator=(const Client &other);

	public:
		Client(int fd, int serverPort, size_t maxBodySize);
		~Client();

		int getFd() const;
		int getServerPort() const;

		IOResult receive();
		IOResult sendData();

		void setResponse(const std::string &response);

		bool hasDataToSend() const;

		const std::string &getRequest() const;
		const std::string &getResponseBuffer() const;

		bool isTimedOut(std::time_t now, int timeout) const;

		void setShouldClose(bool value);
		bool shouldClose() const;

		bool isReadClosed() const;
		void setReadClosed(bool value);
};