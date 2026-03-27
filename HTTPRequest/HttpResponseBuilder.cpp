#include "HttpResponseBuilder.hpp"
#include "HttpResponse.hpp"
#include <sstream>
#include <map>
#include <string>

std::string HttpResponseBuilder::buildResponse(HttpResponse const& response)
{
    std::ostringstream out;
    bool hasContentLength = false;

    out << "HTTP/1.1 " << response.statusCode << " " << response.reasonPhrase << "\r\n";

    for (std::map<std::string, std::string>::const_iterator it = response.headers.begin();
        it != response.headers.end(); ++it)
    {
        if (it->first == "Content-Length")
            hasContentLength = true;
        out << it->first << ": " << it->second << "\r\n";
    }

    if (!hasContentLength)
        out << "Content-Length: " << response.body.size() << "\r\n";

    out << "\r\n";
    out << response.body;
    return out.str();
}