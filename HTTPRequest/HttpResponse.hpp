#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>

class HttpResponse
{
    public:
        int statusCode;
        std::string reasonPhrase;
        std::map<std::string, std::string> headers;
        std::string body;

        HttpResponse();
};

#endif /* HTTPRESPONSE_HPP */
