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
		enum State {
			READING,
			WAITING_FOR_CGI,
			READY_TO_SEND,
			CLOSING
		};

	private:
		int				_fd;
		int				_serverPort;
		std::time_t		_lastActivity;
		std::string		_responseBuffer;

		State			_state;
		bool			_shouldClose;
		
		
		Client();
		Client(const Client &other);
		Client &operator=(const Client &other);

	public:
		Client(int fd, int serverPort, size_t maxBodySize);
		~Client();

		void setState(State state) { _state = state; }
		State getState() const { return _state; }

		int getFd() const;
		int getServerPort() const;

		int receive();
		int sendData();

		void setResponse(const std::string &response);

		bool hasDataToSend() const;

		const std::string &getRequest() const;

		const std::string &getResponseBuffer() const { return _responseBuffer; }

		bool isTimedOut(std::time_t now, int timeout) const;

		void setShouldClose(bool value);
		bool shouldClose() const;
};

