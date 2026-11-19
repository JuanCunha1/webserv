#include "config/ConfigValidator.hpp"

#include <stdexcept>

void ConfigValidator::validate(
    const std::vector<ServerConfig> &servers)
{
    if (servers.empty())
        throw std::runtime_error("ConfigValidator: no server defined");

    for (std::size_t i = 0; i < servers.size(); ++i)
    {
        const ServerConfig &server = servers[i];

        if (server.port < 1 || server.port > 65535)
            throw std::runtime_error("ConfigValidator: invalid server port");

        if (server.host.empty())
            throw std::runtime_error("ConfigValidator: server host cannot be empty");

        if (server.root.empty())
            throw std::runtime_error("ConfigValidator: server root cannot be empty");

        for (std::size_t j = 0; j < server.locations.size(); ++j)
        {
            const LocationConfig &location = server.locations[j];

            if (location.path.empty() || location.path[0] != '/')
                throw std::runtime_error(
                    "ConfigValidator: location path must start with '/'");

            if (location.allowedMethods.empty())
                throw std::runtime_error(
                    "ConfigValidator: location must define allowed_methods");

            for (std::size_t k = j + 1; k < server.locations.size(); ++k)
            {
                if (location.path == server.locations[k].path)
                    throw std::runtime_error(
                        "ConfigValidator: duplicate location: " + location.path);
            }
        }
    }
}
