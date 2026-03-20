#include "HttpResponseBuilder.hpp"
#include "HttpResponse.hpp"
#include <sstream>

std::string HttpResponseBuilder::buildResponse(HttpResponse const& response)
{
    std::ostringstream out;

    out << "HTTP/1.1 " << response.statusCode << " " << response.reasonPhrase << "\r\n";

    for (std::map<std::string, std::string>::const_iterator it = response.headers.begin();
        it != response.headers.end(); ++it)
        out << it->first << ": " << it->second << "\r\n";
    out << "Content-Length: " << response.body.size() << "\r\n";
    out << "\r\n";
    out << response.body;
    return out.str();
}