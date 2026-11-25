#include "../../include/protocol/Response.hpp"


Response::Response()
	: _status(200), _reason("OK"), _version("HTTP/1.1"), _headers(), _body(){}

Response::Response(int status,
			const std::string &reason,
			const std::string &version,
			const std::map<std::string, std::string> &headers,
			const std::string &body)
	:_status(status), _reason(reason), _version(version), _headers(headers), _body(body){}

Response::Response(const Response &other)
	:_status(other._status), _reason(other._reason), _version(other._version), _headers(other._headers), _body(other._body){}

Response &Response::operator=(const Response &other)
{
	if (this != &other)
	{
		this->_status = other._status;
		this->_reason = other._reason;
		this->_version = other._version;
		this->_headers = other._headers;
		this->_body = other._body;
	}
	return(*this);
}

Response::~Response(){}

int Response::getStatus() const
{
	return _status;
}

const std::string &Response::getReason() const
{
	return _reason;
}

const std::string &Response::getVersion() const
{
	return (_version);
}

const std::map<std::string, std::string> &Response::getHeaders() const
{
	return _headers;
}

const std::string &Response::getBody() const
{
	return _body;
}

std::string Response::toString() const
{
	std::ostringstream result;

	result << _version << " " << _status << " " << _reason << "\r\n";
	
	for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it)
	{
		result << it->first << ": " << it->second << "\r\n";
	}
	result << "\r\n" << _body;
	return (result.str());
}
