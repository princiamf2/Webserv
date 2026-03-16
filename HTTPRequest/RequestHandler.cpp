#include "RequestHandler.hpp"
#include <sstream>

static bool isSupportedMethod(std::string const& method)
{
    return (method == "GET" || method == "POST" || method == "DELETE");
}

HttpResponse RequestHandler::handlerResquest(HttpRequest const& request)
{
    HttpResponse response;

    if (request.version != "HTTP/1.1")
    {
        response.statusCode = 505;
        response.reasonPhrase = "HTTP version Not Supported";
        response.headers["Content-Type"] = "text/plain";
        response.body = "505 HTTP Version Not Supported\n";
        return response;
    }

    if (!isSupportedMethod(request.method))
	{
		response.statusCode = 501;
		response.reasonPhrase = "Not Implemented";
		response.headers["Content-Type"] = "text/plain";
		response.body = "501 Not Implemented\n";
		return response;
	}

	if (request.method == "GET")
	{
		response.statusCode = 200;
		response.reasonPhrase = "OK";
		response.headers["Content-Type"] = "text/plain";
		response.body = "GET received for " + request.uri + "\n";
		return response;
	}

	if (request.method == "POST")
	{
		std::ostringstream oss;
        oss << request.body.size();

        response.statusCode = 200;
        response.reasonPhrase = "OK";
        response.headers["Content-Type"] = "text/plain";
        response.body = "POST received with body size = " + oss.str() + "\n";
        return response;
	}

	response.statusCode = 200;
	response.reasonPhrase = "OK";
	response.headers["Content-Type"] = "text/plain";
	response.body = "DELETE received for " + request.uri + "\n";
	return response;
}