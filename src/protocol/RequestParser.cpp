#include "../../include/protocol/RequestParser.hpp"

#include <cctype>
#include <limits>


RequestParser::RequestParser()
	: _buffer(), _maxBodySize(), _complete(), _error(), _request(), _errorMessage(){}

RequestParser::RequestParser(size_t maxBodySize)
	: _buffer(), _maxBodySize(maxBodySize), _complete(), _error(), _request(), _errorMessage(){}

RequestParser::RequestParser(const RequestParser &other)
	: _buffer(other._buffer),
	_maxBodySize(other._maxBodySize),
	_complete(other._complete),
	_error(other._error),
	_request(other._request),
	_errorMessage(other._errorMessage){}

RequestParser &RequestParser::operator=(const RequestParser &other)
{
	if (this != &other)
	{
		this->_buffer = other._buffer;
		this->_maxBodySize = other._maxBodySize;
		this->_complete = other._complete;
		this->_error = other._error;
		this->_request = other._request;
		this->_errorMessage = other._errorMessage;
	}
	return *this;
}

RequestParser::~RequestParser(){}

std::string RequestParser::trim(const std::string &value)
{
	size_t begin = 0;
	size_t end = value.size();

	while (begin < end && (value[begin] == ' '|| value[begin] == '\t'))
		++begin;
	while (end > begin && (value[end -1] == ' '|| value[end - 1] == '\t'))
		--end;
	
	return value.substr(begin, end - begin);
}

std::string RequestParser::toLower(const std::string &value)
{
	std::string result = value;
	
	for (size_t i = 0; i < result.size(); ++i)
	{
		result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
	}
	return (result);
}

bool RequestParser::isNummber(const std::string &value)
{
	if (value .empty())
		return (false);
	for (size_t i = 0 ; i < value.size(); ++i)
	{
		if (!std::isdigit(static_cast<unsigned char>(value[i])))
			return (false);
	}
	return (true);
}

bool RequestParser::parseRequestLine(const std::string &line)
{
	size_t first = line.find(' ');
	size_t second = std::string::npos;

	if (first != std::string::npos)
		second = line.find(' ',first +1);
	if (first == std::string::npos || second == std::string::npos || first == 0 || second == first + 1 || line.find(' ', second + 1) != std::string::npos)
	{
		_errorMessage = "Malformed request line";
		return false;
	}
	std::string method = line.substr(0, first);
	std::string target = line.substr(first + 1, second - first - 1);
	std::string version = line.substr(second + 1);

	if (version != "HTTP/1.0" && version != "HTTP/1.1")
	{
		_errorMessage = "Unsupported HTTP version";
		return(false);
	}

	if (method.empty() || target.empty())
	{
		_errorMessage = "Malformed request line";
		return(false);
	}

	_request = Request(method,target,version, std::map<std::string, std::string>(),"");

	return(true);
}

bool RequestParser::parseHeaderLine(const std::string &line)
{
	size_t colon = line.find(':');

	if (colon == std::string::npos || colon == 0)
	{
		_errorMessage = "Malformed header";
		return false;
	}

	std::string name = toLower(trim(line.substr(0, colon)));
	std::string value = trim(line.substr(colon + 1));

	for (size_t i = 0; i <name.size(); ++i)
	{
		unsigned char c =static_cast<unsigned char>(name[i]);

		if (!std::isalnum(c) && name[i] != '-')
		{
			_errorMessage = "Invalid header name";
			return(false);
		}
	}

	std::map<std::string ,std::string> headers = _request.getHeaders();
	std::map<std::string ,std::string>::iterator found = headers.find(name);

	if (found != headers.end() && name == "content-length")
	{
		if (found->second != value)
		{
			_errorMessage = "Conflicting Content-Length";
			return false;
		}
	}
	else if (found != headers.end())
		found->second += ", " + value;
	else
		headers[name] = value;

	_request = Request(_request.getMethod(), _request.getTarget(), _request.getVersion(), headers, _request.getBody());
	return(true);
}

bool RequestParser::parseHeaders()
{
	size_t end = _buffer.find("\r\n\r\n");

	if (end == std::string::npos)
		return false;
	
	size_t lineEnd = _buffer.find("\r\n");

	if (lineEnd == std::string::npos || !parseRequestLine(_buffer.substr(0, lineEnd)))
		return false;

	size_t lineStrat = lineEnd + 2;

	while (lineStrat < end)
	{
		lineEnd = _buffer.find("\r\n", lineStrat);

		if (lineEnd == std::string::npos || lineEnd > end)
		{
			_errorMessage = "Malformed header block";
			return(false);
		}
		if (!parseHeaderLine(_buffer.substr(lineStrat, lineEnd - lineStrat)))
			return false;
		lineStrat = lineEnd + 2;
	}
	_buffer.erase(0, end + 4);
	return true;
}

size_t RequestParser::transformHexDigit(const std::string &hex)
{
	size_t chunkSize = 0;
	size_t length = hex.size();
	for (size_t i = 0; i < length; ++i)
	{
		unsigned char c = static_cast<unsigned char>(hex[i]);

		int digit;

		if (c >= '0' && c<= '9')
			digit = c - '0';
		else if (c >= 'a' && c <= 'f')
			digit = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			digit = c - 'A' + 10;
		else
			digit = -1;
		if (digit < 0 || chunkSize > (std::numeric_limits<size_t>::max() - digit) / 16)
		{
			_errorMessage = "Invalid chunk size";
			return false ;
		}
		chunkSize = chunkSize *16 + digit;
	}
	return chunkSize;
}

bool RequestParser::checkHeaderConflicts(std::string &lengthText, std::string &transfer)
{
	bool hasTransferEncoding = _request.hasHeader("transfer-encoding");
	bool hasContentLength =_request.hasHeader("content-length");

	if (hasTransferEncoding && hasContentLength)
	{
		_errorMessage = "Content-Length and Transfer-Encoding cannot be used together";
		return false;
	}

	if (hasContentLength && lengthText.empty())
	{
		_errorMessage = "Empty Content-Length";
		return false;
	}

	if (hasTransferEncoding && transfer.empty())
	{
		_errorMessage = "Empty Transfer-Encoding";
		return false;
	}

	if (!transfer.empty() && transfer != "chunked")
	{
		_errorMessage = "Usupported Transfer-Encoding";
		return false;
	}
	return true;
}


bool RequestParser::parseBody()
{
	std::string transfer = toLower(trim(_request.getHeader("transfer-encoding")));
	std::string lengthText = trim(_request.getHeader("content-length"));

	if (!checkHeaderConflicts(lengthText, transfer))
		return false;

	if (transfer == "chunked")
	{
		size_t position = 0;
		std::string body;

		while(true)
		{
			size_t lineEnd = _buffer.find("\r\n", position);

			if (lineEnd == std::string::npos)
				return false;
			std::string sizeText = trim(_buffer.substr(position, lineEnd - position));
			size_t semicolon = sizeText.find(';');

			if (semicolon != std::string::npos)
				sizeText.erase(semicolon);
			
			if (sizeText.empty())
			{
				_errorMessage = "Invalid chunk size";
				return false;
			}

			size_t chunkSize = transformHexDigit(sizeText);

			size_t dataStart = lineEnd + 2;

			if (chunkSize == 0)
			{
				if (_buffer.size() < dataStart + 2)
					return false;
				if (_buffer.compare(dataStart, 2, "\r\n") != 0)
				{
					_errorMessage = "Malformed final chunk";
					return(false);
				}

				_request = Request(_request.getMethod(), _request.getTarget(), _request.getVersion(), _request.getHeaders(), body);

				_buffer.erase(0,dataStart + 2);
				return true;
			}

			if (_buffer.size() < dataStart + chunkSize + 2)
				return(false);
			if (_buffer.compare(dataStart + chunkSize ,2,"\r\n") != 0)
			{
				_errorMessage = "Malformed chunk";
				return(false);
			}

			if (_maxBodySize != 0 && body.size() + chunkSize > _maxBodySize)
			{
				_errorMessage = "Request body too large";
				return(false);
			}

			body.append(_buffer, dataStart, chunkSize);
			position = dataStart + chunkSize + 2;
		}
	}
	if (lengthText.empty())
	{
		_request = Request(_request.getMethod(), _request.getTarget(), _request.getVersion(), _request.getHeaders(), "");
		return(true);
	}

	if (!isNummber(lengthText))
	{
		_errorMessage = "invlid Content-Length";
		return(false);
	}

	size_t length = 0;

	for (size_t i = 0 ;i < lengthText.size(); ++i)
	{
		size_t digit = lengthText[i] - '0';

		if (length > (std::numeric_limits<size_t>::max() - digit) / 10)
		{
			_errorMessage = "Content-Length overflow";
			return false;
		}

		length = length * 10 + digit;
	}

	if (_maxBodySize != 0 && length > _maxBodySize)
	{
		_errorMessage = "Request body too large";
		return false;
	}

	if (_buffer.size() < length)
		return false;

	_request = Request(_request.getMethod(), _request.getTarget(), _request.getVersion(), _request.getHeaders(), _buffer.substr(0 , length));
	_buffer.erase(0,length);
	return(true);
}

void RequestParser::append(const std::string &data)
{
	if (_complete || _error)
		return;
	else if (!_errorMessage.empty())
		_error = true;

	_buffer += data;

	if (_request.getVersion().empty())
	{
		if (_buffer.find("\r\n\r\n") == std::string::npos)
			return;
		if (!parseHeaders())
		{
			_error = true;
			return;
		}
	}
	if (parseBody())
		_complete = true;
	else if (!_errorMessage.empty())
		_error = true;
}

RequestParser::ParseStatus RequestParser::getState() const
{
	if (_error)
		return(PARSE_ERROR);
	if (_complete)
		return(PARSE_COMPLETE);
	return(PARSE_INCOMPLETE);
}

bool RequestParser::isComplete() const
{
	return _complete;
}

bool RequestParser::hasError() const
{
	return _error;
}

const Request &RequestParser::getRequest() const
{
	return _request;
}

const std::string &RequestParser::getErrorMessage() const
{
	return _errorMessage;
}

void RequestParser::reset()
{
	_buffer.clear();
	_complete = false;
	_error = false;
	_request = Request();
	_errorMessage.clear();
}