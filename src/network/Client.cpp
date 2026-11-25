#include "network/Client.hpp"

Client::Client(int fd, int serverPort, size_t maxBodySize)
	: _fd(fd), _serverPort(serverPort)
	, _maxBodySize(maxBodySize)
	, _responseBuffer("")
	, _lastActivity(std::time(NULL))
	, _shouldClose(false)
	, _readClosed(false)
	, _requestHandled(false)
	, _parser(maxBodySize)
{
}

Client::Client()
	: _fd(-1),
	  _serverPort(-1),
	  _maxBodySize(0),
	  _responseBuffer(""),
	  _lastActivity(std::time(NULL)),
	  _shouldClose(false),
	  _readClosed(false),
	  _requestHandled(false),
	  _parser(0)
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

	LOG("[CLIENT " << _fd << "] Waiting for recv()");

	ssize_t bytesRead = recv(_fd, buffer, sizeof(buffer), 0);

	if (bytesRead > 0)
	{
		LOG("[CLIENT " << _fd << "] Received "
				  << bytesRead << " bytes");

		refreshActivityTimer();

		_parser.append(
			std::string(buffer, static_cast<size_t>(bytesRead))
		);

		LOG("[CLIENT " << _fd << "] Parser complete: "
				  << (_parser.isComplete() ? "YES" : "NO"));

		return IO_SUCCESS;
	}

	if (bytesRead == 0)
	{
		LOG("[CLIENT " << _fd << "] Connection closed");
		return IO_CLOSED;
	}

	DEBUG_ERROR("[CLIENT " << _fd << "] recv() returned -1");

	return IO_WOULD_BLOCK;
}

Client::IOResult Client::sendData()
{
	if (_responseBuffer.empty())
		return IO_SUCCESS;

	ssize_t bytesSent = send(
		_fd,
		_responseBuffer.c_str(),
		_responseBuffer.size(),
		0
	);

	if (bytesSent > 0)
	{
		_responseBuffer.erase(0, static_cast<size_t>(bytesSent));
		refreshActivityTimer();
		return IO_SUCCESS;
	}


	return IO_WOULD_BLOCK;
}

bool Client::beginRequestHandling()
{
	if (_requestHandled)
		return false;
	_requestHandled = true;
	return true;
}

bool Client::isTimedOut(std::time_t now, int timeout) const
{
	return (now - _lastActivity) >= timeout;
}

bool Client::hasCompleteRequest() const
{
	return _parser.isComplete();
}

bool Client::hasRequestError() const
{
	return _parser.hasError();
}

const Request &Client::getRequest() const
{
	return _parser.getRequest();
}

const std::string &Client::getRequestError() const
{
	return _parser.getErrorMessage();
}

void Client::resetRequest()
{
	_parser.reset();
	_requestHandled = false;
}

void Client::setResponse(const std::string &response)
{
	_responseBuffer = response;
}

bool Client::hasDataToSend() const
{
	return !_responseBuffer.empty();
}

void Client::setShouldClose(bool value)
{
	_shouldClose = value;
}

bool Client::shouldClose() const
{
	return _shouldClose;
}

void Client::refreshActivityTimer()
{
	_lastActivity = std::time(NULL);
}