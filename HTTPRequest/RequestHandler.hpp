#ifndef REQUESTHANDLER_HPP
#define REQUESTHANDLER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

class RequestHandler
{
    public:
        static HttpResponse handlerResquest(HttpRequest const&);
};

#endif /* REQUESTHANDLER_HPP */
