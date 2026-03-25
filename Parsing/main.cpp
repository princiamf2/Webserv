
#include <iostream>
#include "ParseConfig.hpp"

void print_parse_location(Location const &loc, size_t j) //affiche les informations d'une location  (loc = location a afficher, j = numero de la location dans la liste)
{
    std::cout << "  - Location " << j << std::endl;
    std::cout << "  - path: " << loc.path << std::endl;
    std::cout << "  - root: " << loc.root << std::endl;
    std::cout << "  - methods: ";
    for (std::set<std::string>::const_iterator iter = loc.allowed_methods_http.begin(); //affiche les methodes autorisees pour cette location
         iter != loc.allowed_methods_http.end(); ++iter)
        std::cout << *iter << " ";
    std::cout << std::endl;
    std::cout << "  - index: " << loc.index << std::endl;
    std::cout << "  - show_directory: " << loc.show_directory << std::endl;
    std::cout << "  - upload_dir: " << loc.upload_dir << std::endl;
    std::cout << "  - redirect_page: " << loc.redirect_page.first << " " << loc.redirect_page.second << std::endl;
    std::cout << "  - cgi_extensions: ";
    for (std::set<std::string>::const_iterator iter = loc.cgi_extensions.begin(); //affiche les extensions CGI pour cette location
         iter != loc.cgi_extensions.end(); ++iter)
        std::cout << *iter << " ";
    std::cout << std::endl;
}

void print_parse_server(ServerConfig const &server, size_t i)
{ //affiche les informations d'un serveur (server = serveur a afficher, i = numero du serveur dans la liste)
    std::cout << "Server n° " << i << std::endl;
    std::cout << "root  " << server.root << std::endl;
    std::cout << "domain_names ";
    for (std::set<std::string>::const_iterator iter = server.domain_names.begin();
         iter != server.domain_names.end(); ++iter)
        std::cout << *iter << " ";
    std::cout << std::endl;
    std::cout << "ports ";
    for (std::set<unsigned int>::const_iterator iter = server.listen_ports.begin();
         iter != server.listen_ports.end(); ++iter)
        std::cout << *iter << " ";
    std::cout << std::endl;
    std::cout << "index: " << server.index << std::endl;
    std::cout << "client_max_body_size: " << server.client_max_body_size << std::endl;
    for (std::map<int, std::string>::const_iterator iter = server.error_pages.begin(); //affiche les pages d'erreur du serveur (code d'erreur et chemin de la page d'erreur)
         iter != server.error_pages.end(); ++iter)
        std::cout << "error_page: " << iter->first << " " << iter->second << std::endl;
    for (size_t j = 0; j < server.locations.size(); j++)
        print_parse_location(server.locations[j], j);
}

void printServers(std::vector<ServerConfig> servers) //affiche les informations de tous les serveurs (servers = liste des serveurs a afficher)
{
    std::cout << "nb de servers: " << servers.size() << std::endl;
    for (size_t i = 0; i < servers.size(); ++i)
        print_parse_server(servers[i], i);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << " ./webserv configfile_path" << std::endl;
        return 1;
    }
    std::vector<ServerConfig> servers = parseConfig(argv[1]);
    printServers(servers);
    return 0;
}

// TODO (ROBUSTESSE DU PARSING GLOBAL):
// Le parsing actuel fonctionne uniquement si la configuration est parfaitement formatée,
// mais il est fragile face aux variations ou aux erreurs de syntaxe.
//
// Problèmes à corriger:
//
// 1. Détection des blocs:
//    - parseConfig utilise line.find("server") → trop permissif.
//    - Il faut détecter explicitement "server" comme mot-clé suivi de "{".
//    - Éviter les faux positifs (ex: "myserver", commentaires, etc.).
//
// 2. Gestion des accolades { }:
//    - parseServer et parseLocation s'arrêtent sur un simple find("}").
//    - Cela ne garantit pas que les blocs sont correctement imbriqués.
//    - Il faut:
//        - vérifier l'ouverture explicite avec "{"
//        - suivre correctement les niveaux d'imbrication
//        - s'assurer que chaque "{" a un "}" correspondant
//
// 3. Parsing des locations:
//    - location.path est lu sans vérifier la présence du "{".
//    - Il faut valider la syntaxe:
//          location /path {
//              ...
//          }
//
// 4. Sensibilité aux espaces et format:
//    - le parsing dépend fortement du format exact (espaces, retours à la ligne).
//    - il faut:
//        - ignorer les lignes vides
//        - ignorer les espaces en début/fin
//        - tolérer des formats légèrement différents
//
// 5. Découpage des lignes:
//    - le parsing se fait ligne par ligne sans vérifier la structure complète.
//    - certaines directives pourraient être mal interprétées si mal formatées.
//
// 6. stripSemicolon:
//    - suppose que ";" est toujours présent et bien placé.
//    - ne vérifie pas si le ";" est absent ou mal positionné.
//
// 7. Lecture partielle des directives:
//    - certaines fonctions lisent seulement une partie de la ligne sans vérifier qu'elle est complète.
//    - ex: domain_name ne lit qu'une valeur.
//
// 8. Absence de validation structurelle:
//    - aucune vérification globale de cohérence:
//        - server sans contenu
//        - location mal fermée
//        - directives hors bloc
//
// Conclusion:
// Le parseur doit être rendu plus strict et structuré:
//   - parsing basé sur des blocs bien définis (server/location)
//   - validation explicite des ouvertures/fermetures
//   - suppression des heuristiques basées sur find()
//   - indépendance vis-à-vis du formatage (espaces, lignes)
//
// Sans ces corrections, le parseur acceptera des configurations invalides
// ou plantera sur des cas pourtant valides.

