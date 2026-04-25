#ifndef HTTPRESPONSEBUILDER_HPP
#define HTTPRESPONSEBUILDER_HPP

#include "HttpResponse.hpp"

class HttpResponseBuilder
{
    public:
        static std::string buildResponse(HttpResponse const&);
    private:
        HttpResponseBuilder();
        HttpResponseBuilder(HttpResponseBuilder const&);
        HttpResponseBuilder& operator=(HttpResponseBuilder const&);
        ~HttpResponseBuilder();
};

#endif /* HTTPRESPONSEBUILDER_HPP */
