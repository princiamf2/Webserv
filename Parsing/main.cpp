
#include <iostream>
#include "ParseConfig.hpp"


int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << " ./webserv configfile_path" << std::endl;
        return 1;
    }
    std::vector<ServerConfig> servers = parseConfig(argv[1]);

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
    }

    std::cout << "ports ";
    for (unsigned int port : servers[0].listen_ports)
        std::cout << port << " ";
    std::cout << std::endl;
    
    return 0;
}
