#include "HttpParser.hpp"
#include <sstream>
#include <stdexcept>

static std::string trim(const std::string& str)
{
    size_t start = 0;
    size_t end = str.size();

    while (start < str.size() && (str[start] == ' ' || str[start] == '\t' || str[start] == '\r'))
        start++;
    while (end > start && (str[end - 1] == ' ' || str[end - 1] == '\t' || str[end - 1] == '\r'))
        end--;
    return str.substr(start, end - start);
}

HttpRequest HttpParser::parserRequest(std::string const& rawRequest)
{
    HttpRequest request;
    size_t headerEnd = rawRequest.find("\r\n\r\n");

    if (headerEnd == std::string::npos)
        throw std::runtime_error("Invalid HTTP request: missing header/body separator");
    std::string headerPart = rawRequest.substr(0, headerEnd);
    request.body = rawRequest.substr(headerEnd + 4);

    std::istringstream stream(headerPart);
    std::string line;
    if (!std::getline(stream, line))
        throw std::runtime_error("Invalid HTTP request: empty request");
    if (!line.empty() && line[line.size() - 1] == '\r')
        line.erase(line.size() - 1);
    std::istringstream requestLine(line);
    if (!(requestLine >> request.method >> request.uri >> request.version))
        throw std::runtime_error("Invalid HTTP request line");
    while (std::getline(stream, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos)
            throw std::runtime_error("Invalid header line");
        std::string key = trim(line.substr(0, colonPos));
        std::string value = trim(line.substr(colonPos + 1));
        request.headers[key] = value;
    }
    return request;
}