#include "HttpParser.hpp"
#include "RequestHandler.hpp"
#include "HttpResponseBuilder.hpp"
#include <iostream>

int main()
{
	std::string rawRequest =
		"GET /index.html HTTP/1.1\r\n"
		"Host: localhost:8080\r\n"
		"\r\n";

	try
	{
		HttpRequest request = HttpParser::parseRequest(rawRequest);
		HttpResponse response = RequestHandler::handleRequest(request);
		std::string rawResponse = HttpResponseBuilder::buildResponse(response);

		std::cout << "===== RESPONSE =====" << std::endl;
		std::cout << rawResponse << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
	}
	return 0;
}