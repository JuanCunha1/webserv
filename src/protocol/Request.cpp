#include "../../include/protocol/Request.hpp"

#include <cctype>

Request::Request()
    : _method(), _target(), _version(), _headers(), _body(){}

Request::Request(const std::string &method,
				const std::string &target,
				const std::string &version,
				const std::map<std::string, std::string> &headers,
				const std::string &body)
    : _method(method), _target(target), _version(version), _headers(headers), _body(body){}

Request::Request(const Request &other)
    : _method(other._method), _target(other._target), _version(other._version), _headers(other._headers), _body(other._body){}

Request &Request::operator=(const Request &other)
{
    if (this != &other)
    {
        this->_method = other._method;
        this->_target = other._target;
        this->_version = other._version;
        this->_headers = other._headers;
        this->_body = other._body;
    }
    return(*this);
}

Request::~Request(){}

const std::string &Request::getMethod() const
{
    return(_method);
}
const std::string &Request::getTarget() const
{
    return(_target);
}
const std::string &Request::getVersion() const
{
    return(_version);
}
const std::map<std::string, std::string> &Request::getHeaders() const
{
    return(_headers);
}
const std::string &Request::getBody() const
{
    return(_body);
}

std::string Request::lowerName(const std::string &name) const
{
    std::string result = name;
    
    for (size_t i = 0; i < result.size(); ++i)
    {
        result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
    }
    return (result);
}

bool Request::hasHeader(const std::string &name) const
{
    return (_headers.find(lowerName(name)) != _headers.end());
}

std::string Request::getHeader(const std::string &name) const
{
    std::map<std::string, std::string>::const_iterator it;
    it = _headers.find(lowerName(name));
    if (it == _headers.end())
        return "";
    return (it->second);
}

