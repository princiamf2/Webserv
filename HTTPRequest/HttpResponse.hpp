#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>

//la class response que lui contien aussi un constructeur
class HttpResponse
{
    public:
        int statusCode;
        std::string reasonPhrase;
        std::map<std::string, std::string> headers;
        std::string body;

        HttpResponse();
        HttpResponse(HttpResponse const&);
        HttpResponse& operator=(HttpResponse const&);
        ~HttpResponse();
};

#endif /* HTTPRESPONSE_HPP */
