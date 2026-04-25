#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>

//class request qui contien que des membres
class HttpRequest
{
    public:
        std::string method;
        std::string uri;
        std::string path;
        std::string query;
        std::string version;
        std::map<std::string, std::string> headers;
        std::string body;
        
        HttpRequest();
        HttpRequest(HttpRequest const&);
        HttpRequest& operator=(HttpRequest const&);
        ~HttpRequest();
};

#endif /* HTTPREQUEST_HPP */
