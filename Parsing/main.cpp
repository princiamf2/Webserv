
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
    return 0;
}
