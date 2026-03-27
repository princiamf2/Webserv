
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
    std::cout << "  - cgi_interpreters: ";
    for (std::map<std::string, std::string>::const_iterator iter = loc.cgi_interpreters.begin();
        iter != loc.cgi_interpreters.end(); ++iter)
    {
        std::cout << "(" << iter->first << " -> " << iter->second << ") ";
    }
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


// TODO (VALIDATION PLUS STRICTE / AMELIORATIONS POSSIBLES):
// Le parsing actuel est fonctionnel et déjà robuste pour le niveau attendu,
// mais il peut encore être rendu plus strict sur certains points.
//
// Améliorations possibles :
//
// 1. Directives obligatoires
//    - Vérifier qu'un bloc server contient au moins un listen.
//    - Éventuellement imposer certaines directives minimales selon la logique voulue.
//
// 2. Duplication de directives
//    - Refuser les directives dupliquées si elles ne doivent apparaître qu'une fois.
//    - Exemples possibles :
//         root
//         index
//         client_max_body_size
//         show_directory
//         upload_dir
//
// 3. Validation plus stricte des chemins
//    - Vérifier que les chemins filesystem (root, upload_dir, error_page path) sont cohérents.
//    - Vérifier que les paths HTTP de location commencent bien par '/'.
//
// 4. Validation plus stricte des extensions CGI
//    - Vérifier qu'une extension CGI commence bien par '.'.
//    - Exemples valides : .php, .py
//
// 5. Gestion stricte des ';'
//    - Vérifier explicitement que les directives qui doivent finir par ';' le possèdent bien.
//    - Éviter qu'une ligne incomplète soit acceptée silencieusement.
//
// 6. Arguments en trop
//    - Après parsing d'une directive, vérifier qu'il ne reste pas de tokens inattendus.
//    - Exemple à refuser :
//         listen 8080 extra;
//         root /var/www extra;
//
// 7. Duplication de valeurs dans certaines listes
//    - Optionnellement refuser les doublons dans methods ou cgi_extensions,
//      au lieu de simplement les absorber dans un std::set.
//
// 8. Validation de fin de bloc plus sémantique
//    - Vérifier qu'un bloc server ou location n'est pas vide si cela doit être interdit.
//    - Vérifier qu'une location possède les éléments minimaux attendus si nécessaire.
//
// 9. Cohérence logique entre directives
//    - Optionnellement vérifier des incohérences de configuration.
//    - Exemples :
//         upload_dir sans méthode POST
//         cgi_extensions sans logique CGI derrière
//
// 10. Messages d'erreur encore plus précis
//    - Ajouter si besoin le nom exact de la directive fautive,
//      voire le contenu de la ligne, pour faciliter le debug.
//
// Ces points ne remettent pas en cause le parsing actuel,
// mais correspondent à une version encore plus stricte et défensive.