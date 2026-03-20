#ifndef HTTPRESPONSEBUILDER_HPP
#define HTTPRESPONSEBUILDER_HPP

#include "HttpResponse.hpp"

class HttpResponseBuilder
{
    public:
        static std::string buildResponse(HttpResponse const&);
};

#endif /* HTTPRESPONSEBUILDER_HPP */
