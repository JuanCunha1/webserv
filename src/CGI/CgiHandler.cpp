#include "CGI/CgiHandler.hpp"

#include <sstream>
#include <sys/wait.h>
#include <cstdlib>
#include <cctype>

CgiHandler::CgiHandler(size_t maxOutputSize)
	: _process(maxOutputSize),
	  _state(CGI_NOT_STARTED),
	  _httpStatus(200),
	  _httpReason("OK"),
	  _httpResponse()
{
}

CgiHandler::~CgiHandler()
{
}

void CgiHandler::start(const std::string &interpreter,
					   const std::string &scriptPath,
					   const std::vector<std::string> &environment,
					   const std::string &requestBody)
{
	if (_state != CGI_NOT_STARTED)
		throw std::runtime_error("CGI handler has already been started");

	_process.start(interpreter, scriptPath, environment, requestBody);
	_state = CGI_RUNNING;
}

CgiHandler::IOResult CgiHandler::handleStdinWritable()
{
	if (_state != CGI_RUNNING)
		return IO_ERROR;

	CgiProcess::IOResult result = _process.writeInput();

	if (result == CgiProcess::IO_SUCCESS)
		return IO_SUCCESS;
	if (result == CgiProcess::IO_CLOSED)
		return IO_CLOSED;

	_state = CGI_FAILED;
	return IO_ERROR;
}

CgiHandler::IOResult CgiHandler::handleStdoutReadable()
{
	if (_state != CGI_RUNNING)
		return IO_ERROR;

	CgiProcess::IOResult result = _process.readOutput();

	if (result == CgiProcess::IO_SUCCESS)
		return IO_SUCCESS;
	if (result == CgiProcess::IO_CLOSED)
		return IO_CLOSED;
	if (result == CgiProcess::IO_LIMIT_EXCEEDED)
	{
		_state = CGI_FAILED;
		return IO_OUTPUT_LIMIT;
	}

	_state = CGI_FAILED;
	return IO_ERROR;
}

bool CgiHandler::checkChildStatus()
{
	if (_state != CGI_RUNNING && _state != CGI_FAILED)
		return _process.hasChildExited();

	try
	{
		return _process.checkChildStatus();
	}
	catch (...)
	{
		_state = CGI_FAILED;
		throw;
	}
}

bool CgiHandler::finalize()
{
	if (_state != CGI_RUNNING)
		return false;

	if (!_process.isFinished())
		return false;

	if (!WIFEXITED(_process.getExitStatus()) ||
		WEXITSTATUS(_process.getExitStatus()) != 0)
	{
		_state = CGI_FAILED;
		return false;
	}

	if (!parseCgiOutput())
	{
		_state = CGI_FAILED;
		return false;
	}

	_state = CGI_FINISHED;
	return true;
}

void CgiHandler::terminate()
{
	if (_state == CGI_RUNNING)
	{
		_process.terminate();
		_state = CGI_FAILED;
	}
}

std::string CgiHandler::trim(const std::string &value)
{
	size_t first = 0;
	size_t last = value.size();

	while (first < last &&
		   (value[first] == ' ' || value[first] == '\t' ||
			value[first] == '\r' || value[first] == '\n'))
		++first;

	while (last > first &&
		   (value[last - 1] == ' ' || value[last - 1] == '\t' ||
			value[last - 1] == '\r' || value[last - 1] == '\n'))
		--last;

	return value.substr(first, last - first);
}

bool CgiHandler::isValidHeaderName(const std::string &name)
{
	if (name.empty())
		return false;

	for (size_t i = 0; i < name.size(); ++i)
	{
		unsigned char c = static_cast<unsigned char>(name[i]);

		if (!std::isalnum(c) && c != '!' && c != '#' && c != '$' &&
			c != '%' && c != '&' && c != '\'' && c != '*' &&
			c != '+' && c != '-' && c != '.' && c != '^' &&
			c != '_' && c != '`' && c != '|' && c != '~')
			return false;
	}
	return true;
}

bool CgiHandler::isHopByHopHeader(const std::string &name)
{
	std::string lower;
	for (size_t i = 0; i < name.size(); ++i)
		lower += static_cast<char>(
			std::tolower(static_cast<unsigned char>(name[i])));

	return lower == "connection" ||
		   lower == "keep-alive" ||
		   lower == "proxy-authenticate" ||
		   lower == "proxy-authorization" ||
		   lower == "te" ||
		   lower == "trailer" ||
		   lower == "transfer-encoding" ||
		   lower == "upgrade" ||
		   lower == "content-length";
}

std::string CgiHandler::statusReason(int status)
{
	switch (status)
	{
		case 200: return "OK";
		case 201: return "Created";
		case 202: return "Accepted";
		case 204: return "No Content";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 303: return "See Other";
		case 304: return "Not Modified";
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 408: return "Request Timeout";
		case 413: return "Payload Too Large";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
		case 503: return "Service Unavailable";
		case 504: return "Gateway Timeout";
		default: return "CGI Response";
	}
}

bool CgiHandler::parseStatusHeader(const std::string &value)
{
	std::istringstream stream(value);
	int status = 0;

	if (!(stream >> status) || status < 100 || status > 599)
		return false;

	std::string reason;
	std::getline(stream, reason);
	reason = trim(reason);

	_httpStatus = status;
	_httpReason = reason.empty() ? statusReason(status) : reason;
	return true;
}

bool CgiHandler::parseCgiOutput()
{
	const std::string &output = _process.getOutput();

	size_t separator = output.find("\r\n\r\n");
	size_t separatorLength = 4;

	if (separator == std::string::npos)
	{
		separator = output.find("\n\n");
		separatorLength = 2;
	}

	if (separator == std::string::npos)
		return false;

	std::string rawHeaders = output.substr(0, separator);
	std::string body = output.substr(separator + separatorLength);

	std::vector<std::string> headers;
	size_t lineStart = 0;

	while (lineStart <= rawHeaders.size())
	{
		size_t lineEnd = rawHeaders.find('\n', lineStart);
		if (lineEnd == std::string::npos)
			lineEnd = rawHeaders.size();

		std::string line = rawHeaders.substr(lineStart, lineEnd - lineStart);
		line = trim(line);

		if (!line.empty())
			headers.push_back(line);

		if (lineEnd == rawHeaders.size())
			break;
		lineStart = lineEnd + 1;
	}

	bool hasContentType = false;
	bool hasLocation = false;
	std::string responseHeaders;

	_httpStatus = 200;
	_httpReason = "OK";

	for (size_t i = 0; i < headers.size(); ++i)
	{
		size_t colon = headers[i].find(':');
		if (colon == std::string::npos)
			return false;

		std::string name = trim(headers[i].substr(0, colon));
		std::string value = trim(headers[i].substr(colon + 1));

		if (!isValidHeaderName(name))
			return false;

		std::string lowerName;
		for (size_t j = 0; j < name.size(); ++j)
			lowerName += static_cast<char>(
				std::tolower(static_cast<unsigned char>(name[j])));

		if (lowerName == "status")
		{
			if (!parseStatusHeader(value))
				return false;
			continue;
		}

		if (lowerName == "content-type")
			hasContentType = true;
		if (lowerName == "location")
			hasLocation = true;

		if (isHopByHopHeader(name))
			continue;

		if (value.find('\r') != std::string::npos ||
			value.find('\n') != std::string::npos)
			return false;

		responseHeaders += name;
		responseHeaders += ": ";
		responseHeaders += value;
		responseHeaders += "\r\n";
	}

	if (!hasContentType && !hasLocation)
		return false;

	if (hasLocation && _httpStatus == 200)
	{
		_httpStatus = 302;
		_httpReason = "Found";
	}

	std::ostringstream response;
	response << "HTTP/1.1 " << _httpStatus << " " << _httpReason << "\r\n";
	response << responseHeaders;
	response << "Content-Length: " << body.size() << "\r\n";
	response << "Connection: close\r\n";
	response << "\r\n";
	response << body;

	_httpResponse = response.str();
	return true;
}

int CgiHandler::getStdinFd() const
{
	return _process.getStdinFd();
}

int CgiHandler::getStdoutFd() const
{
	return _process.getStdoutFd();
}

pid_t CgiHandler::getPid() const
{
	return _process.getPid();
}

CgiHandler::State CgiHandler::getState() const
{
	return _state;
}

bool CgiHandler::isRunning() const
{
	return _state == CGI_RUNNING;
}

bool CgiHandler::isFinished() const
{
	return _state == CGI_FINISHED;
}

bool CgiHandler::isFailed() const
{
	return _state == CGI_FAILED;
}

bool CgiHandler::isComplete() const
{
	return _process.isFinished();
}

const std::string &CgiHandler::getCgiOutput() const
{
	return _process.getOutput();
}

const std::string &CgiHandler::getHttpResponse() const
{
	return _httpResponse;
}

int CgiHandler::getHttpStatus() const
{
	return _httpStatus;
}

const std::string &CgiHandler::getHttpReason() const
{
	return _httpReason;
}

bool CgiHandler::hasInputToSend() const
{
	return _process.hasInputToSend();
}

bool CgiHandler::isStdinClosed() const
{
	return _process.isStdinClosed();
}

bool CgiHandler::isStdoutClosed() const
{
	return _process.isStdoutClosed();
}

bool CgiHandler::hasChildExited() const
{
	return _process.hasChildExited();
}

std::time_t CgiHandler::getStartTime() const
{
	return _process.getStartTime();
}