#include "protocol/ResponseBuilder.hpp"

Response ResponseBuilder::build(const std::string &version ,int status, 
								const std::string &reason, const std::string &body, 
								const std::string &contentType, bool closeConnection)
{
	std::map<std::string, std::string> headers;
	std::ostringstream length;

	length << body.size();

	
	headers["Content-Length"] = length.str();
	if (!contentType.empty())
		headers["Content-Type"] = contentType;
	if (closeConnection)
		headers["Connection"] = "close";
	else
		headers["Connection"] = "keep-alive";
	return(Response(status, reason, version, headers, body));
}

Response ResponseBuilder::ok(const std::string &version ,const std::string &body, const std::string &contentType)
{
	return(build(version, 200, "OK", body, contentType, false));
}

Response ResponseBuilder::error(const std::string &version ,int status, const std::string &reason)
{
	return(build(version, status, reason, reason + "\n", "text/plain", true));
}

Response ResponseBuilder::head(const std::string &version,
							   const std::string &body,
							   const std::string &contentType,
							   bool closeConnection)
{
	Response response = build(
		version,
		200,
		"OK",
		body,
		contentType,
		closeConnection
	);

	return Response(
		response.getStatus(),
		response.getReason(),
		response.getVersion(),
		response.getHeaders(),
		""
	);
}