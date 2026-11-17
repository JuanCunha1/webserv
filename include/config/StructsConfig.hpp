#include <string>
#include <vector>
#include <map>

struct LocationConfig {
	std::string path;
	std::string root;
	std::string index;
	std::string uploadPath;
	std::vector<std::string> allowedMethods;
	bool autoindex;
	LocationConfig()
		: autoindex(false)
	{
	}
};

struct ServerConfig {
	std::string host;
	std::string serverName;
	std::string root;
	std::string index;
	std::map<int, std::string> errorPages;
	int port;
	size_t maxBodySize;
	std::vector<LocationConfig> locations;
};


