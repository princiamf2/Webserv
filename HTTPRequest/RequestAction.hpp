#ifndef REQUESTACTION_HPP
#define REQUESTACTION_HPP

#include <cstddef>
# include <string>
# include "HttpRequest.hpp"
# include "HttpResponse.hpp"
# include "../Parsing/Location.hpp"

enum RequestActionType
{
    ACTION_IMMEDIATE_RESPONSE,
    ACTION_START_CGI
};

struct RequestAction
{
    RequestActionType type;
    HttpResponse response;
    HttpRequest request;
    const Location* location;
    std::string scriptPath;
    std::string interpreter;

    RequestAction() : type(ACTION_IMMEDIATE_RESPONSE), location(NULL) {}
};

#endif /* REQUESTACTION_HPP */
