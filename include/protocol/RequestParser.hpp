#pragma once

#include <cstddef>
#include <string>

#include "Request.hpp"

class RequestParser
{
	public:
		
		enum ParseStatus
		{
			PARSE_INCOMPLETE,
			PARSE_COMPLETE,
			PARSE_ERROR
		};
	
	private:
		std::string _buffer;
		size_t      _maxBodySize;
		bool        _complete;
		bool        _error;
		Request     _request;
		std::string _errorMessage;

		bool parseHeaders();
		bool parseRequestLine(const std::string &line);
		bool parseHeaderLine(const std::string &line);
		bool parseBody();

		static std::string trim(const std::string &value);
		static std::string toLower(const std::string &value);
		static bool isNummber(const std::string &value);

		size_t	transformHexDigit(const std::string &hex);
		bool	checkHeaderConflicts(std::string &lengthText, std::string &transfer);


	public:
		
		RequestParser();
		RequestParser(size_t maxBodySize);
		RequestParser(const RequestParser &other);
		RequestParser &operator=(const RequestParser &other);
		~RequestParser();

		void	append(const std::string &data);

		ParseStatus getState() const;
		bool	isComplete() const;
		bool	hasError() const;

		const	Request &getRequest() const;
		const	std::string &getErrorMessage() const;

		void	reset();
};