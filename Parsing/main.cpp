
#include <iostream>
#include "ParseConfig.hpp"

void print_parse_location(Location const &loc, size_t j)
{
    std::cout << "  - Location " << j << std::endl;
    std::cout << "  - path: " << loc.path << std::endl;
    std::cout << "  - root: " << loc.root << std::endl;
    std::cout << "  - methods: ";
    for (std::set<std::string>::const_iterator iter = loc.allowed_methods_http.begin();
         iter != loc.allowed_methods_http.end(); ++iter)
        std::cout << *iter << " ";
    std::cout << std::endl;
    std::cout << "  - index: " << loc.index << std::endl;
    std::cout << "  - show_directory: " << loc.show_directory << std::endl;
    std::cout << "  - upload_dir: " << loc.upload_dir << std::endl;
    std::cout << "  - redirect_page: " << loc.redirect_page.first << " " << loc.redirect_page.second << std::endl;
    std::cout << "  - cgi_extensions: ";
    for (std::set<std::string>::const_iterator iter = loc.cgi_extensions.begin();
         iter != loc.cgi_extensions.end(); ++iter)
        std::cout << *iter << " ";
    std::cout << std::endl;
}

void print_parse_server(ServerConfig const &server, size_t i)
{
    std::cout << "Server " << i << std::endl;
    std::cout << "root " << server.root << std::endl;
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
    for (std::map<int, std::string>::const_iterator iter = server.error_pages.begin();
         iter != server.error_pages.end(); ++iter)
        std::cout << "error_page: " << iter->first << " " << iter->second << std::endl;
    for (size_t j = 0; j < server.locations.size(); j++)
        print_parse_location(server.locations[j], j);
}

void printServers(std::vector<ServerConfig> servers)
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