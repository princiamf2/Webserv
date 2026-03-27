// HTTPRequest/HttpModule.cpp
#include "HttpModule.hpp"
#include "HttpParser.hpp"
#include "RequestHandler.hpp"
#include "HttpResponseBuilder.hpp"
#include <cstddef>

// choisit la location la plus précise : plus long préfixe qui match
static bool locationMatches(std::string const& requestPath, std::string const& locationPath)
{
	if (locationPath.empty())
		return false;
	if (requestPath == locationPath)
		return true;
	if (requestPath.find(locationPath) != 0)
		return false;
	if (locationPath[locationPath.size() - 1] == '/')
		return true;
	if (requestPath.size() > locationPath.size()
		&& requestPath[locationPath.size()] == '/')
		return true;
	return false;
}

static Location const* findBestLocation(ServerConfig const& server, std::string const& path)
{
	Location const* best = NULL;
	size_t bestLen = 0;

	for (size_t i = 0; i < server.locations.size(); ++i)
	{
		Location const& loc = server.locations[i];

		if (locationMatches(path, loc.path) && loc.path.size() > bestLen)
		{
			best = &server.locations[i];
			bestLen = loc.path.size();
		}
	}
	return best;
}

// transforme toute erreur interne en réponse HTTP exploitable
static std::string buildSimpleErrorResponse(int code,
                                            std::string const& reason,
                                            std::string const& body)
{
    HttpResponse response;
    response.statusCode = code;
    response.reasonPhrase = reason;
    response.body = body;
    response.headers["Content-Type"] = "text/plain";
    return HttpResponseBuilder::buildResponse(response);
}

std::string handleRawHttpRequest(std::string const& rawRequest,
                                 ServerConfig const& server)
{
    try
    {
        HttpRequest request = HttpParser::parseRequest(rawRequest);
        Location const* location = findBestLocation(server, request.path);
        HttpResponse response = RequestHandler::handleRequest(request, server, location);
        return HttpResponseBuilder::buildResponse(response);
    }
    catch (const std::exception& e)
    {
        return buildSimpleErrorResponse(400, "Bad Request", e.what());
    }
}