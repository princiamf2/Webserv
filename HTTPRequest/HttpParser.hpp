#ifndef HTTPPARSER_HPP
#define HTTPPARSER_HPP

#include "HttpRequest.hpp"

// class avec juste une methode static pour l'instant pour parser les requete

class HttpParser
{
    public:
        static HttpRequest parseRequest(std::string const&);

		HttpParser();
		HttpParser(HttpParser const&);
		HttpParser& operator=(HttpParser const&);
		~HttpParser();
};

#endif /* HTTPPARSER_HPP */
