
#include "ParseConfig.hpp"
#include <fstream> //permet de lire le fichier de config
#include <iostream>
#include <sstream> //permet de lire le contenu du configfile ligne par ligne


//funct qui parse configfile et retourne un vector de ServerConfig
std::vector<ServerConfig> parseConfig(std::string path)
{
	std::ifstream configFile(path);
	if(!configFile.is_open())
	{
		std::cerr << "Error opening config file " << path << std::endl;
		return std::vector<ServerConfig>();
	}
	
	std::string content((std::istreambuf_iterator<char>(configFile)), std::istreambuf_iterator<char>()); // lit tout le contenu du configfile dans une string
	//ifstreambuf_iterator parcourt un fichier char par char, premier pour le debut du fichier, second pour la fin du fichier
	if (content.empty())
	{
	    std::cerr << "config file is empty" << std::endl;
    	return std::vector<ServerConfig>();
	}

	// A FAIRE: analyser le contenu et remplir les ServerConfig
	std::istringstream stream(content); //charge le content 
	std::string line;
	std::vector<ServerConfig> serverlist; //vector de ServerConfig a retourner
	
	while (std::getline(stream, line)) //boucle principale pour lire le configfile ligne par ligne
	{
		if (line.find("server") != std::string::npos)
		{
			ServerConfig server;	
			std::string serverLine;
			while (std::getline(stream, serverLine)) 
			{
				if (serverLine.find("}") != std::string::npos)
					break;
				//A FAIRE : analyser serveLine et remplir server
				std::istringstream lineStream(serverLine);
				std::string word_to_parse;
				lineStream >> word_to_parse;
				if (word_to_parse == "listen")
				{
					std::string port_str;
					lineStream >> port_str; // extaire le port par exemple "0808;"
					port_str = port_str.substr(0, port_str.find(";"));
					std::istringstream iss(port_str);
					unsigned int port;
					iss >> port;
					if (iss.fail())
					{
						std::cerr << "invalid port: " << port_str << std::endl;
						return std::vector<ServerConfig>();
					}
					//unsigned int port = std::stoul(port_str); // stoul convert str en UINT
					server.listen_ports.insert(port); 
				}
				else if (word_to_parse == "root")
				{
					std::string root_path;
					lineStream >> root_path; //recup path de root
					root_path = root_path.substr(0, root_path.find(";")); // enleve le ;
					server.root = root_path; // assigne a server.root
				}
				else if (word_to_parse == "domain_name")
				{
					std::string domain_name;
					lineStream >> domain_name; //recup domain_name
					domain_name = domain_name.substr(0, domain_name.find(";"));
					server.domain_names.insert(domain_name); //std::set donc insert pour ajouter a server.domain_names
				}
				/*
				else if (word_to_parse == "index") {}
				else if (word_to_parse == "error_page") {}
				else if (word_to_parse == "client_max_body_size") {}
				*/
				else if (word_to_parse == "location") 
				{
					Location location;
					std::string locationLine;
					lineStream >> location.path;
					while (std::getline(stream, locationLine)) //seconde boucle pour lire les lignes location
					{
						if (locationLine.find("}") != std::string::npos)
							break;
						std::istringstream locationStream(locationLine);
						std::string location_word;
						locationStream >> location_word;
						/*
						if (location_word == "root") {}
						else if (location_word == "index") {}
						else if (location_word == "methods") {}
						else if (location_word == "show_directory") {}
						else if (location_word == "upload_dir") {}
						else if (location_word == "redirect_page") {}
						else if (location_word == "cgi_extensions") {}
						*/
					}
					
				}
				
			}
			serverlist.push_back(server);
		}
		std::cout << "Line: " << line << std::endl; 
	}
	return serverlist;
}



//exemple a parser

/*

server
{
    listen 8080;
    root /var/www/html;
    domain_name webserv.com;
    location /images
    {
        root /var/www/images;
        methods GET;
    }
}

*/

/*

c++ -Wall -Wextra -Werror main.cpp ParseConfig.cpp -o webserv

*/