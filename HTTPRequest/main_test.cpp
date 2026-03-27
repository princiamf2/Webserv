#include "HttpParser.hpp"
#include "RequestHandler.hpp"
#include "HttpResponseBuilder.hpp"
#include "../Parsing/ParseConfig.hpp"
#include <iostream>
#include <vector>

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

static Location const* findBestLocation(ServerConfig const& server,
										std::string const& path)
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
int main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cerr << "Usage: ./webserv_test_http config_file" << std::endl;
		return 1;
	}

	std::vector<ServerConfig> servers = parseConfig(argv[1]);
	if (servers.empty())
	{
		std::cerr << "Error: no server config loaded" << std::endl;
		return 1;
	}

	ServerConfig const& server = servers[0];

	std::vector<std::string> rawRequests;
	rawRequests.push_back(
		"GET /index.html HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n"
	);
	rawRequests.push_back(
		"GET /images/logo.png HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n"
	);
	rawRequests.push_back(
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"Content-Length: 11\r\n"
		"\r\n"
		"hello world"
	);
	rawRequests.push_back(
		"DELETE /images/logo.png HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n"
	);
	rawRequests.push_back(
		"DELETE /upload/upload.txt HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n"
	);

	for (size_t i = 0; i < rawRequests.size(); ++i)
	{
		std::cout << "===== TEST " << i + 1 << " =====" << std::endl;

		try
		{
			HttpRequest request = HttpParser::parseRequest(rawRequests[i]);
			const Location* location = findBestLocation(server, request.path);

			HttpResponse response = RequestHandler::handleRequest(request, server, location);
			std::string rawResponse = HttpResponseBuilder::buildResponse(response);

			std::cout << rawResponse << std::endl;
		}
		catch (const std::exception& e)
		{
			std::cerr << "Error: " << e.what() << std::endl;
		}

		std::cout << std::endl;
	}

	return 0;
}
