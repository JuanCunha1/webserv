#include "../../../include/protocol/handlers/GetHandler.hpp"
#include "../../../include/protocol/handlers/HandlerUtils.hpp"
#include "../../../include/protocol/ResponseBuilder.hpp"

#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

static std::string htmlEscape(const std::string &value)
{
	std::string result;
	for (size_t i = 0; i <value.size(); ++i)
	{
		if (value[i] == '&')
			result += "&amp;";
		else if(value[i] == '<')
			result += "&lt;";
		else if(value[i] == '>')
			result += "&gt;";
		else if(value[i] == '"')
            result += "&quot;";
		else
			result += value[i];
	}
	return result;
}

static bool makeDirectoryListing(const std::string &path, const std::string &uri, std::string &body)
{
	DIR *directory = opendir(path.c_str());
	if (directory == NULL)
		return(false);
	std::ostringstream html;
	html << "<!doctype html><html><body><h1>Index of " << htmlEscape(uri) << "</h1><ul>\n";
	errno = 0;
	struct dirent *entry;
	while ((entry = readdir(directory)) != NULL)
	{
		std::string name =entry->d_name;
		if(name == "." || name == "..")
			continue;
		html << "<li>" << htmlEscape(name) << "</li>\n";
	}
	int readError = errno;
	int closeResult = closedir(directory);
	if (readError != 0 || closeResult == -1)
		return(false);
	html << "</ul></body></html>\n";
	body = html.str();
	return true;
}

Response GetHandler::handle(const Request &request, const ServerConfig &server, const LocationConfig &location)
{
	std::string path;
	if (!HandlerUtils::buildResourcePath(request, server, location, path))
		return(ResponseBuilder::error(request.getVersion(), 403,"Forbidden"));
	struct stat info;
	if (stat(path.c_str(), &info) == -1)
	{
		if (errno == ENOENT || errno == ENOTDIR)
			return(ResponseBuilder::error(request.getVersion(), 404, "Not Found"));
		return(ResponseBuilder::error(request.getVersion(), 403,"Forbidden"));
	}
	if (S_ISDIR(info.st_mode))
	{
		std::string index ;
		if (location.index.empty())
			index = server.index;
		else
			index = location.index;
		if (!index.empty())
		{
			if (path[path.size() - 1] != '/')
				path += '/';
			path += index;
			if(stat(path.c_str(), &info) == -1)
				return(ResponseBuilder::error(request.getVersion(), 404, "Not Found"));
		}
		else if (location.autoindex)
		{
			std::string listing;
			if (!makeDirectoryListing(path, HandlerUtils::uriPath(request), listing))
				return(ResponseBuilder::error(request.getVersion(), 500, "Internal Server Error"));
			return(ResponseBuilder::build(request.getVersion(),200, "OK" , listing ,"text/html", HandlerUtils::closeConnection(request)));
		}
		else 
			return(ResponseBuilder::error(request.getVersion(), 403, "Forbidden"));
	}
	if (!S_ISREG(info.st_mode))
			return(ResponseBuilder::error(request.getVersion(), 403, "Forbidden"));
	int fd = open(path.c_str(), O_RDONLY);
	if (fd == -1)
	{
		if (errno == ENOENT || errno == ENOTDIR)
			return(ResponseBuilder::error(request.getVersion(), 404, "Not Found"));
		if (errno == EACCES)
			return(ResponseBuilder::error(request.getVersion(), 403, "Forbidden"));
		return(ResponseBuilder::error(request.getVersion(), 500, "Internal Server Error"));
	}
	std::string body;
	char buffer[4096];
	bool readFailed = false;
	while (true)
	{
		ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
		if (bytesRead > 0)
			body.append(buffer, static_cast<size_t>(bytesRead));
		else if (bytesRead == 0)
			break;
		else if(errno != EINTR)
		{
			readFailed = true;
			break;
		}
	}
	int closeResult = close(fd);
	if (readFailed || closeResult == -1)
		return(ResponseBuilder::error(request.getVersion(), 500, "Internal Server Error"));
	return ResponseBuilder::build(request.getVersion(),200, "OK" , body , HandlerUtils::contentType(path), HandlerUtils::closeConnection(request));	
}