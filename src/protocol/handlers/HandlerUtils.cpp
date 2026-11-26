#include "../../../include/protocol/handlers/HandlerUtils.hpp"


std::string HandlerUtils::uriPath(const Request &request)
{
    std::string target = request.getTarget();
    size_t query = target.find('?');

    if (query != std::string::npos)
        target.erase(query);

    return(target);
}

bool HandlerUtils::hasUnsafeSegment(const std::string &path)
{
    size_t start = 0;

    while (start <= path.size())
    {
        size_t end = path.find('/',start);
        std::string segment;

        if (end == std::string::npos)
            segment = path.substr(start);
        else
            segment = path.substr(start, end - start);
        
        if (segment == "." || segment == "..")
            return(true);
        if (end == std::string::npos)
            break;

        start = end +1;
    }
    return(false);
}

bool HandlerUtils::buildResourcePath(const Request &request, const ServerConfig &server, const LocationConfig &location, std::string &filePath)
{
    std::string uri =uriPath(request);
    if (uri.empty() || uri[0] != '/' || hasUnsafeSegment(uri))
        return(false);
    std::string root;
    if(location.root.empty())
        root = server.root;
    else
        root = location.root;
    std::string relative = uri;
    if (!location.root.empty())
    {
        if(uri.compare(0, location.path.size(), location.path) != 0)
            return(false);
        relative = uri.substr(location.path.size());
    }
    while (!relative.empty() && relative[0] == '/')
        relative.erase(0,1);
    filePath = root;
    if (!filePath.empty() && filePath[filePath.size() - 1] != '/')
        filePath += '/';
    filePath += relative;
    return(true);
}

bool HandlerUtils::buildUploadPath(const Request &request, const LocationConfig &location, std::string &filePath)
{
    if (location.uploadPath.empty())
        return false;
    std::string uri = uriPath(request);
    if (uri.empty() || uri [0] != '/' || hasUnsafeSegment(uri))
        return(false);
    std::string prefix = location.path;
    if(prefix.empty())
        prefix = "/";
    std::string filename;
    if(prefix == "/")
        filename = uri.substr(1);
    else
    {
        if(uri.compare(0 ,prefix.size(), prefix) != 0)
            return(false);
        if (uri.size() <= prefix.size() || uri[prefix.size()] != '/')
            return(false);
        filename = uri.substr(prefix.size() + 1);
    }
    if (filename.empty()  || filename.find('/') != std::string::npos || filename == "." || filename == "..")
        return(false);
    for (size_t i = 0; i < filename.size(); ++i)
    {
        if (std::iscntrl(static_cast<unsigned char>(filename[i])))
            return(false);
    }
    filePath = location.uploadPath;
    if (filePath[filePath.size() - 1] != '/')
        filePath += '/';
    filePath += filename;
    return true;
}

bool HandlerUtils::closeConnection(const Request &request)
{
    std::string connection = request.getHeader("connection");

    for (size_t i = 0; i <connection.size(); ++i)
    {
        connection[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(connection[i])));
    }

    if (request.getVersion() == "HTTP/1.0" || request.getVersion() == "HTTP/1.1")
        return connection != "keep-alive";
    return(connection == "close");
}

std::string HandlerUtils::contentType(const std::string &path)
{
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return("application/octet-stream");
    std::string extension = path.substr(dot);
    for (size_t i = 0; i < extension.size(); ++i)
    {
        extension[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(extension[i])));
    }
    if (extension == ".html" || extension == ".htm")
        return "text/html";
    if (extension == ".css")
        return "text/css";
    if (extension == ".js")
        return "application/javascript";
    if (extension == ".txt")
        return "text/plain";
    if (extension == ".json")
        return "application/json";
    if (extension == ".png")
        return "image/png";
    if (extension == ".jpg" || extension == ".jpeg")
        return "image/jpeg";
    if (extension == ".gif")
        return "image/gif";
    if (extension == ".svg")
        return "image/svg+xml";
    return("application/octet-stream");
}
