#ifndef HTTPPARSER_HPP
#define HTTPPARSER_HPP

#include "HttpRequest.hpp"

class HttpParser
{
    public:
        static HttpRequest parserRequest(std::string const&);
};

#endif /* HTTPPARSER_HPP */
