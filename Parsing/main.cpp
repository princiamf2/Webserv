
#include <iostream>
#include "ParseConfig.hpp"


void printServers(std::vector<ServerConfig> servers)
{
    std::cout << "nb de servers: " << servers.size() << std::endl;
    for (size_t i = 0; i < servers.size(); ++i)
    {
        std::cout << "Server " << i << std::endl;
        std::cout << "root " << servers[i].root << std::endl;
        std::cout << "domain_names ";
        for (std::set<std::string>::iterator iter = servers[i].domain_names.begin();
            iter != servers[i].domain_names.end(); ++iter)
            std::cout << *iter << " ";
        std::cout << std::endl;
        std::cout << "ports ";
        for (std::set<unsigned int>::iterator iter = servers[i].listen_ports.begin();
            iter != servers[i].listen_ports.end(); ++iter)
            std::cout << *iter << " ";
        std::cout << std::endl;
        for (size_t j = 0; j < servers[i].locations.size(); j++)
        {
            std::cout << "  Location " << j << std::endl;
            std::cout << "  path: " << servers[i].locations[j].path << std::endl;
            std::cout << "  root: " << servers[i].locations[j].root << std::endl;
            std::cout << "  methods: ";
            for (std::set<std::string>::iterator it = servers[i].locations[j].allowed_methods_http.begin();
                 it != servers[i].locations[j].allowed_methods_http.end(); ++it)
                std::cout << *it << " ";
            std::cout << std::endl;
        }
    }
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