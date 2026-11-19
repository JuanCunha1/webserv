#pragma once

#include <map>
#include <string>

class Request
{
	private:
		std::string _method;
		std::string _target;
		std::string _version;
		std::map<std::string, std::string> _headers;
		std::string _body;

		std::string lowerName(const std::string &name) const;

	public:
		Request();
		Request(const std::string &method,
				const std::string &target,
				const std::string &version,
				const std::map<std::string, std::string> &headers,
				const std::string &body);
        Request(const Request &other);
        Request &operator=(const Request &other);
        ~Request();

		const std::string &getMethod() const;
		const std::string &getTarget() const;
		const std::string &getVersion() const;
		const std::map<std::string, std::string> &getHeaders() const;
		const std::string &getBody() const;

		bool hasHeader(const std::string &name) const;
		std::string getHeader(const std::string &name) const;
};