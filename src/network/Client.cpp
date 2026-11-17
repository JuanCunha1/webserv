#include "network/Client.hpp"

Client::Client(int fd, int serverPort, size_t maxBodySize)
	: _fd(fd), _serverPort(serverPort), _lastActivity(std::time(NULL))
	, _responseBuffer(""), _state(READING)
	, _shouldClose(false)
{
	(void)maxBodySize; // Evitar advertencia de variable no utilizada
}

Client::Client()
	: _fd(-1), _serverPort(-1), _lastActivity(std::time(NULL))
	, _responseBuffer(""), _state(READING)
	, _shouldClose(false)
{
}

Client::Client(const Client &other)
	: _fd(other._fd)
	, _lastActivity(other._lastActivity)
	, _responseBuffer(other._responseBuffer)
	
{
}

Client &Client::operator=(const Client &other)
{
	if (this != &other)
	{
		_fd = other._fd;
		_lastActivity = other._lastActivity;
		_responseBuffer = other._responseBuffer;
		
	}
	return *this;
}

Client::~Client()
{
	if (_fd != -1)
		close(_fd);

	//! Creo que debo hacerlo
}

int Client::getServerPort() const
{
	return _serverPort;
}

int Client::getFd() const
{
	return _fd;
}

int Client::receive()
{
	char buffer[4096];

	ssize_t bytesRead = recv(
		_fd,
		buffer,
		sizeof(buffer),
		0
	);

	if (bytesRead > 0)
	{
	//	_parser.append(std::string(buffer, bytesRead));
		_lastActivity = std::time(NULL);
		return static_cast<int>(bytesRead);
	}

	if (bytesRead == 0)
		return 0;

	if (errno == EAGAIN || errno == EWOULDBLOCK)
		return -1;

	return -2;
}

void Client::setResponse(const std::string &response)
{
	_responseBuffer = response;
}

bool Client::hasDataToSend() const
{
	return !_responseBuffer.empty();
}

int Client::sendData()
{
	if (_responseBuffer.empty())
		return 0;

	ssize_t bytesSent = send(
		_fd,
		_responseBuffer.c_str(),
		_responseBuffer.size(),
		0
	);

	if (bytesSent > 0)
	{
		_responseBuffer.erase(0, bytesSent);
		_lastActivity = std::time(NULL);
		return static_cast<int>(bytesSent);
	}

	if (bytesSent == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 0;

		return -1;
	}

	return -1;
}

bool Client::isTimedOut(std::time_t now, int timeout) const
{
	return (now - _lastActivity) >= timeout;
}

void Client::setShouldClose(bool value)
{
	_shouldClose = value;
}

bool Client::shouldClose() const
{
	return _shouldClose;
}