#include "network/Client.hpp"

Client::Client(int fd, int serverPort, size_t maxBodySize)
	: _fd(fd), _serverPort(serverPort)
	, _maxBodySize(maxBodySize)
	, _responseBuffer("")
	, _lastActivity(std::time(NULL))
	, _shouldClose(false)
	, _readClosed(false)
{
}

Client::Client()
	: _fd(-1),
	  _serverPort(-1),
	  _maxBodySize(0),
	  _requestBuffer(""),
	  _responseBuffer(""),
	  _lastActivity(std::time(NULL)),
	  _shouldClose(false),
	  _readClosed(false)
{
}

Client::~Client()
{
	if (_fd != -1)
	{
		close(_fd);
		_fd = -1;
	}
}

int Client::getServerPort() const
{
	return _serverPort;
}

int Client::getFd() const
{
	return _fd;
}

bool Client::isReadClosed() const
{
	return _readClosed;
}

void Client::setReadClosed(bool value)
{
	_readClosed = value;
}

Client::IOResult Client::receive()
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
		_requestBuffer = buffer;
		//_requestBuffer.append(buffer, bytesRead);
		_lastActivity = std::time(NULL);

		return IO_SUCCESS;
	}

	if (bytesRead == 0)
		return IO_CLOSED;

	return IO_ERROR;
}

void Client::setResponse(const std::string &response)
{
	_responseBuffer = response;
}

bool Client::hasDataToSend() const
{
	return !_responseBuffer.empty();
}

Client::IOResult Client::sendData()
{
	if (_responseBuffer.empty())
		return IO_SUCCESS;

	ssize_t bytesSent = send(
		_fd,
		_responseBuffer.c_str(),
		_responseBuffer.size(),
		MSG_NOSIGNAL
	);

	if (bytesSent > 0)
	{
		_responseBuffer.erase(0, bytesSent);
		_lastActivity = std::time(NULL);

		return IO_SUCCESS;
	}

	return IO_ERROR;
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

const std::string &Client::getRequest() const
{
    return _requestBuffer;
}