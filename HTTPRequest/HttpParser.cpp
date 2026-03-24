#include "HttpParser.hpp"
#include <sstream>
#include <stdexcept>

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
    if (!(requestLine >> request.method >> request.uri >> request.version))//je mets chaque partie de la line ou il faut car une requete doit toujours commecer par la method le uri et la version
        throw std::runtime_error("Invalid HTTP request line");             //pour le moment je verifie pas si les methode sont corect etc...

    //ici je lis le reste de hearder et je cherche : qui est le separateur key value
    while (std::getline(stream, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos)
            throw std::runtime_error("Invalid header line");
        std::string key = trim(line.substr(0, colonPos));
        std::string value = trim(line.substr(colonPos + 1));
        request.headers[key] = value;
    }
    return request;
}

// TODO:
// Le parsing HTTP est incomplet et incorrect sur plusieurs points:
//
// 1. Fin des headers:
//    Une requête HTTP sépare les headers du body par une ligne vide ("\r\n").
//    Il faut arrêter la lecture des headers dès que line == "".
//    Sinon, le body est interprété comme un header -> erreur.
//
// 2. Lecture du body:
//    request.body n'est jamais rempli actuellement.
//    Il faut lire le reste du stream après les headers.
//
// 3. Content-Length:
//    Pour les requêtes POST, il faut lire exactement le nombre d'octets indiqué
//    dans le header "Content-Length". Sinon, le body peut être incomplet ou trop lu.
//
// 4. Header Host:
//    En HTTP/1.1, le header "Host" est obligatoire.
//    Il faudra vérifier sa présence et sinon retourner une erreur 400.
//
// 5. URI:
//    L'URI est stockée brute.
//    Il faudra plus tard la découper en:
//       - path (pour le routing)
//       - query string (optionnelle)
//
// Sans ces corrections, le parsing HTTP est non conforme et instable.
//nico