#pragma once

#include <map>
#include <string>
#include <sstream>

class Response
{
    private:
        int _status;
        std::string _reason;
        std::string _version;
        std::map<std::string, std::string> _headers;
        std::string _body;
    
    public:
        Response();
        Response(int status,
                const std::string &reason,
                const std::string &version,
                const std::map<std::string, std::string> &headers,
                const std::string &body);
        Response(const Response &other);
        Response &operator=(const Response &other);
        ~Response();
        
        int getStatus() const;
        const std::string &getReason() const;
        const std::string &getVersion() const;
        const std::map<std::string, std::string> &getHeaders() const;
        const std::string &getBody() const;

        std::string toString() const;
};
