
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
					// A FAIRE : enlever le ; a la fin
					//convertir en unsigned int
					//ajouter a server.listen_ports
				}
				else if (word_to_parse == "root")
				{
					std::string root_path;
					lineStream >> root_path; //recup path de root
					// A FAIRE : enlever le ; a la fin
					//ajouter a server.root
				}
				else if (word_to_parse == "domain_name")
				{
					std::string domain_name;
					lineStream >> domain_name; //recup domain_name
					// A FAIRE : enlever le ; a la fin
					//ajouter a server.domain_name
				}
				/*
				else if (word_to_parse == "index") {}
				else if (word_to_parse == "error_page") {}
				else if (word_to_parse == "client_max_body_size") {}
				else if (word_to_parse == "location") {}
				*/
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
}

*/

/*

c++ -Wall -Wextra -Werror main.cpp ParseConfig.cpp -o webserv

*/