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
};

#endif /* HTTPREQUEST_HPP */

// TODO:
// L'URI est actuellement stockée brute dans "uri".
// Cela pose problème pour le routing et le filesystem.
//
// Il faut ajouter:
//   - path : partie avant '?'
//   - query : partie après '?'
//
// Exemple:
//   URI: /images/logo.png?size=small
//   path = /images/logo.png
//   query = size=small
//
// Cela permettra:
//   - un routing correct avec les Location
//   - un accès fichier propre sans polluer avec la query
//nico
