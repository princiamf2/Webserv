#include "HttpResponseBuilder.hpp"
#include "HttpResponse.hpp"
#include <sstream>
#include <map>
#include <string>

// canonic
HttpResponseBuilder::HttpResponseBuilder() {}
HttpResponseBuilder::HttpResponseBuilder(HttpResponseBuilder const& other) {(void)other;}
HttpResponseBuilder& HttpResponseBuilder::operator=(HttpResponseBuilder const& other) {(void)other; return *this;}
HttpResponseBuilder::~HttpResponseBuilder() {}

std::string HttpResponseBuilder::buildResponse(HttpResponse const& response)
{
    std::ostringstream out;
    bool hasContentLength = false;
    bool hasConnection = false;
    bool hasServer = false;

    out << "HTTP/1.1 " << response.statusCode << " " << response.reasonPhrase << "\r\n";

    for (std::map<std::string, std::string>::const_iterator it = response.headers.begin();
        it != response.headers.end(); ++it)
    {
        if (it->first == "Content-Length")
            hasContentLength = true;
        if (it->first == "Connection")
            hasConnection = true;
        if (it->first == "Server")
            hasServer = true;
        out << it->first << ": " << it->second << "\r\n";
    }

    if (!hasServer)
        out << "Server: webserv\r\n";
    if (!hasConnection)
        out << "Connection: close\r\n";
    if (!hasContentLength)
        out << "Content-Length: " << response.body.size() << "\r\n";

    out << "\r\n";
    out << response.body;
    return out.str();
}
