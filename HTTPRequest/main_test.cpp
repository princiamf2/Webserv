#include "HttpParser.hpp"
#include "RequestHandler.hpp"
#include "HttpResponseBuilder.hpp"
#include "../Parsing/ParseConfig.hpp"
#include <iostream>
#include <vector>
#include <string>

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

static void runOneTest(ServerConfig const& server,
	std::string const& label,
	std::string const& rawRequest)
{
	std::cout << "========================================" << std::endl;
	std::cout << "TEST: " << label << std::endl;
	std::cout << "--------------- REQUEST ----------------" << std::endl;
	std::cout << rawRequest << std::endl;
	std::cout << "-------------- RESPONSE ----------------" << std::endl;

	try
	{
		HttpRequest request = HttpParser::parseRequest(rawRequest);
		Location const* location = findBestLocation(server, request.path);
		HttpResponse response = RequestHandler::handleRequest(request, server, location);
		std::string rawResponse = HttpResponseBuilder::buildResponse(response);
		std::cout << rawResponse << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cout << "Parser exception: " << e.what() << std::endl;
	}

	std::cout << std::endl;
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

	std::vector< std::pair<std::string, std::string> > tests;

	// ----------- GET simple file -----------
	tests.push_back(std::make_pair(
		"GET existing index file",
		"GET /index.html HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n"
	));

	tests.push_back(std::make_pair(
		"GET existing image file",
		"GET /images/logo.png HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n"
	));

	tests.push_back(std::make_pair(
		"GET missing file -> 404",
		"GET /images/missing.png HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n"
	));

	// ----------- directory / index / autoindex -----------
	tests.push_back(std::make_pair(
		"GET directory with index",
		"GET /docs HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n"
	));

	tests.push_back(std::make_pair(
		"GET autoindex directory",
		"GET /browse HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n"
	));

	// ----------- redirect -----------
	tests.push_back(std::make_pair(
		"GET redirect location",
		"GET /old-page HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n"
	));

	// ----------- POST upload -----------
	tests.push_back(std::make_pair(
		"POST upload normal",
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"Content-Length: 11\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"hello world"
	));

	tests.push_back(std::make_pair(
		"POST body too large",
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"Content-Length: 40\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"1234567890123456789012345678901234567890"
	));

	// ----------- DELETE -----------
	tests.push_back(std::make_pair(
		"DELETE existing upload file",
		"DELETE /upload/upload_0.txt HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n"
	));

	tests.push_back(std::make_pair(
		"DELETE missing file -> 404",
		"DELETE /upload/not_here.txt HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n"
	));

	// ----------- security -----------
	tests.push_back(std::make_pair(
		"GET traversal attempt -> forbidden",
		"GET /images/../../secret.txt HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n"
	));

	// ----------- parser errors -----------
	tests.push_back(std::make_pair(
		"Missing Host header -> parser error",
		"GET /index.html HTTP/1.1\r\n"
		"\r\n"
	));

	tests.push_back(std::make_pair(
		"Bad Content-Length -> parser error",
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"Content-Length: abc\r\n"
		"\r\n"
		"hello"
	));

	tests.push_back(std::make_pair(
		"Body size mismatch -> parser error",
		"POST /upload HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"Content-Length: 10\r\n"
		"\r\n"
		"hello"
	));

	tests.push_back(std::make_pair(
		"Bad path without leading slash -> parser error",
		"GET index.html HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n"
	));

	// ----------- CGI GET -----------
	tests.push_back(std::make_pair(
		"GET CGI python with query",
		"GET /cgi-bin/test.py?name=michel&x=42 HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n"
	));

	tests.push_back(std::make_pair(
		"POST CGI python with body",
		"POST /cgi-bin/test.py?name=michel HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"Content-Length: 11\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"hello world"
	));

	for (size_t i = 0; i < tests.size(); ++i)
		runOneTest(server, tests[i].first, tests[i].second);

	return 0;
}