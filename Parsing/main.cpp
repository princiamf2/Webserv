
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
		std::cout << "index: " << servers[i].index << std::endl;
		std::cout << "client_max_body_size: " << servers[i].client_max_body_size << std::endl;
		
		for (std::map<int, std::string>::iterator iter = servers[i].error_pages.begin();
    		iter != servers[i].error_pages.end(); ++iter)
    	std::cout << "error_page: " << iter->first << " " << iter->second << std::endl;

        for (size_t j = 0; j < servers[i].locations.size(); j++)
        {
            std::cout << "  Location " << j << std::endl;
            std::cout << "  path: " << servers[i].locations[j].path << std::endl;
            std::cout << "  root: " << servers[i].locations[j].root << std::endl;
            std::cout << "  methods: ";

            for (std::set<std::string>::iterator iter = servers[i].locations[j].allowed_methods_http.begin();
                iter != servers[i].locations[j].allowed_methods_http.end(); ++iter)
                std::cout << *iter << " ";
            std::cout << std::endl;

            std::cout << "  index: " << servers[i].locations[j].index << std::endl;
            std::cout << "  show_directory: " << servers[i].locations[j].show_directory << std::endl;
            std::cout << "  upload_dir: " << servers[i].locations[j].upload_dir << std::endl;
            std::cout << "  redirect_page: " << servers[i].locations[j].redirect_page.first << " " << servers[i].locations[j].redirect_page.second << std::endl;
            std::cout << "  cgi_extensions: ";

            for (std::set<std::string>::iterator iter = servers[i].locations[j].cgi_extensions.begin();
                iter != servers[i].locations[j].cgi_extensions.end(); ++iter)
                std::cout << *iter << " ";
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