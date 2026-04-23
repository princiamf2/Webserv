#ifndef REQUESTHANDLER_HPP
#define REQUESTHANDLER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "RequestAction.hpp"
#include "../Parsing/ServerConfig.hpp"

class RequestHandler
{
    public:
        static HttpResponse handleRequest(HttpRequest const&, ServerConfig const&, Location const*);
        static ActionRequest resolveAction(HttpRequest const&, ServerConfig const&, Location const*);
    private:
        RequestHandler();
        RequestHandler(RequestHandler const&);
        RequestHandler& operator=(RequestHandler const&);
        ~RequestHandler();
};

#endif /* REQUESTHANDLER_HPP */
