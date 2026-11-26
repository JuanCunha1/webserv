#include "../../../include/protocol/handlers/DeleteHandler.hpp"
#include "../../../include/protocol/handlers/HandlerUtils.hpp"
#include "../../../include/protocol/ResponseBuilder.hpp"

#include <cerrno>
#include <map>
#include <sys/stat.h>
#include <unistd.h>

Response DeleteHandler::handle(const Request &request, const LocationConfig &location )
{
    std::string path;

    if (!HandlerUtils::buildUploadPath(request, location, path))
		return(ResponseBuilder::error(request.getVersion(), 403,"Forbidden"));
    struct stat info;
    if (stat(path.c_str(), &info) == -1)
	{
		if (errno == ENOENT || errno == ENOTDIR)
			return(ResponseBuilder::error(request.getVersion(), 404, "Not Found"));
		if (errno == EACCES || errno == EPERM)
    		return(ResponseBuilder::error(request.getVersion(), 403,"Forbidden"));
		return(ResponseBuilder::error(request.getVersion(), 500, "Internal Server Error"));
    }
    if(!S_ISREG(info.st_mode))
    	return(ResponseBuilder::error(request.getVersion(), 403,"Forbidden"));
    if (unlink(path.c_str()) == -1)
    {
        if (errno == ENOENT)
		    return(ResponseBuilder::error(request.getVersion(), 404, "Not Found"));
        if (errno == EACCES || errno == EPERM || errno == EISDIR)
			return(ResponseBuilder::error(request.getVersion(), 403, "Forbidden"));
		return(ResponseBuilder::error(request.getVersion(), 500, "Internal Server Error"));
    }
    std::map<std::string, std::string> headers;
	if (HandlerUtils::closeConnection(request))
		headers["Connection"] = "close";
	else
		headers["Connection"] = "keep-alive";
	return Response(204,"No Content", request.getVersion(), headers, "");
}