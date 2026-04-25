#include "HttpParser.hpp"
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <cstdio>
#include <cctype>

// canonic
HttpParser::HttpParser() {}
HttpParser::HttpParser(HttpParser const& other) {(void)other;}
HttpParser& HttpParser::operator=(HttpParser const& other) {(void)other; return *this;}
HttpParser::~HttpParser() {}

// petit outil pour retirer espace au debut et a la fin
static std::string trim(const std::string& str)
{
    size_t start = 0;
    size_t end = str.size();

    while (start < str.size() && (str[start] == ' ' || str[start] == '\t' || str[start] == '\r'))
        start++;
    while (end > start && (str[end - 1] == ' ' || str[end - 1] == '\t' || str[end - 1] == '\r'))
        end--;
    return str.substr(start, end - start);
}

static std::string toLowerString(std::string s)
{
    for (size_t i = 0; i < s.size(); ++i)
        s[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    return s;
}

//separe uri en path et query
static void splitUri(std::string const& uri, std::string& path, std::string& query)
{
    size_t pos = uri.find('?');

    if (pos == std::string::npos)
    {
        path = uri;
        query.clear();
        return;
    }
    path = uri.substr(0, pos);
    query = uri.substr(pos + 1);
}

static size_t HttpParserContentLength(std::string const& value)
{
    std::istringstream iss(value);
    size_t length;

    if (!(iss >> length))
        throw std::runtime_error("Invalid Content-Length");
    iss >> std::ws;
    if (iss.peek() != EOF)
        throw std::runtime_error("Invalid Content-Length");
    return length;
}

HttpRequest HttpParser::parseRequest(std::string const& rawRequest)
{
    HttpRequest request;//une requete
    size_t headerEnd = rawRequest.find("\r\n\r\n");//je prends la position du separateur entre header et body

    if (headerEnd == std::string::npos)//si il existe pas je renvoi une exception une requete dois toujours avoir ce separateur
        throw std::runtime_error("Invalid HTTP request: missing header/body separator");

    std::string headerPart = rawRequest.substr(0, headerEnd);//je stock la partie header de la requete du debut de la requte jusqu'au separateur
    request.body = rawRequest.substr(headerEnd + 4);//et je stock le body dans request.body car la s'est le header qui m'interresse

    std::istringstream stream(headerPart);//ici je transforme la partie header en flux ce qui veux dire que ça va lire la ligne un peux comme un file en prenant chaque mot separe par des espace meme multiple
    std::string line;//la variable que je vais utiliser dans getline

    if (!std::getline(stream, line))//je lie la premiere ligne si il y a rien a lire je renvoi une erreur
        throw std::runtime_error("Invalid HTTP request: empty request");
    if (!line.empty() && line[line.size() - 1] == '\r')//je regarde si la ligne que j'ai lu a '\r' a la fin ou il est pas vide
        line.erase(line.size() - 1);//si il a '\r' je l'enleve

    std::istringstream requestLine(line);//je transforme la ligne en flux
    if (!(requestLine >> request.method >> request.uri >> request.version))
        throw std::runtime_error("Invalid HTTP request line");
    requestLine >> std::ws;
    if (requestLine.peek() != EOF)
        throw std::runtime_error("Invalid HTTP request line");//pour le moment je verifie pas si les methode sont corect etc...

    splitUri(request.uri, request.path, request.query);
    if (request.path.empty())
        throw std::runtime_error("Invalid HTTP request empty path");
    if (request.path[0] != '/')
        throw std::runtime_error("Invalid HTTP request: path must start with /");

    //ici je lis le reste de hearder et je cherche : qui est le separateur key value
    while (std::getline(stream, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos)
            throw std::runtime_error("Invalid header line");
        std::string key = trim(line.substr(0, colonPos));
        key = toLowerString(key);
        std::string value = trim(line.substr(colonPos + 1));
        request.headers[key] = value;
    }
    if (request.version == "HTTP/1.1"
        && request.headers.find("host") == request.headers.end())
        throw std::runtime_error("Invalid HTTP request: missing Host header");
    std::map<std::string, std::string>::const_iterator it = request.headers.find("content-length");
    if (it != request.headers.end())
    {
        size_t expected = HttpParserContentLength(it->second);
        if (request.body.size() != expected)
            throw std::runtime_error("Invalid HTTP request: body size does not match Content-Length");
    }
    return request;
}
