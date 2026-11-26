#include "../../../include/protocol/handlers/PostHandler.hpp"
#include "../../../include/protocol/handlers/HandlerUtils.hpp"
#include "../../../include/protocol/ResponseBuilder.hpp"

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

Response PostHandler::handle(const Request &request, const LocationConfig &location)
{
    std::string path;
    if(!HandlerUtils::buildUploadPath(request, location, path))
		return(ResponseBuilder::error(request.getVersion(), 403,"Forbidden"));
    struct stat info;
    bool existed = false;
    if(stat(path.c_str(), &info) == 0)
    {
        if (!S_ISREG(info.st_mode))
			return(ResponseBuilder::error(request.getVersion(), 403, "Forbidden"));
		existed = true;
    }
	else if (errno != ENOENT)
	{
		if (errno == EACCES || errno == EPERM)
			return(ResponseBuilder::error(request.getVersion(), 403, "Forbidden"));
		return(ResponseBuilder::error(request.getVersion(), 500, "Internal Server Error"));
	}
	int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1)
	{
		if(errno == EACCES || errno == EPERM)
			return(ResponseBuilder::error(request.getVersion(), 403, "Forbidden"));
		return(ResponseBuilder::error(request.getVersion(), 500, "Internal Server Error"));
	}
	const std::string &body = request.getBody();
	size_t written = 0;
	bool writeFailed = false;
	while (written < body.size())
	{
		ssize_t bytesWritten = write(fd, body.data() + written, body.size() - written);
		if (bytesWritten  > 0)
			written += static_cast<size_t>(bytesWritten);
		else if (bytesWritten == -1 && errno == EINTR)
			continue;
		else
		{
			writeFailed = true;
			break;
		}
	}
	int closeResult = close(fd);
	if(writeFailed ||closeResult == -1)
		return(ResponseBuilder::error(request.getVersion(), 500, "Internal Server Error"));
	if(existed)
		return(ResponseBuilder::build(request.getVersion(),200, "OK" , "File repalaced\n" ,"text/plain", HandlerUtils::closeConnection(request)));
	return(ResponseBuilder::build(request.getVersion(),201, "Created" , "File created\n" ,"text/plain", HandlerUtils::closeConnection(request)));
}