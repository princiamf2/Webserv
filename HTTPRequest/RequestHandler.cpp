#include "RequestHandler.hpp"
#include <sstream>

//petit outil qui check les methods
static bool isSupportedMethod(std::string const& method)
{
    return (method == "GET" || method == "POST" || method == "DELETE");
}

//on fait une validation et on met les code d'erreur et les message d'erreur
HttpResponse RequestHandler::handleRequest(HttpRequest const& request)
{
    HttpResponse response;

	//si pas bonne version
    if (request.version != "HTTP/1.1")
    {
        response.statusCode = 505;
        response.reasonPhrase = "HTTP version Not Supported";
        response.headers["Content-Type"] = "text/plain";
        response.body = "505 HTTP Version Not Supported\n";
        return response;
    }

	//si s'est pas une method que nous supportons
    if (!isSupportedMethod(request.method))
	{
		response.statusCode = 501;
		response.reasonPhrase = "Not Implemented";
		response.headers["Content-Type"] = "text/plain";
		response.body = "501 Not Implemented\n";
		return response;
	}

	//et la on va faire les reponse de chaque methode
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