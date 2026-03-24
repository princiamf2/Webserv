
#include "ParseConfig.hpp"
#include <fstream> //permet de lire le fichier de config
#include <iostream>
#include <sstream> //permet de lire le contenu du configfile ligne par ligne


std::string stripSemicolon(const std::string& s) //pour enlever le ; a la fin de chaque ligne
{
	return s.substr(0, s.find(";"));
}

std::vector<ServerConfig> parseConfig(std::string path)
{
	std::ifstream configFile(path.c_str()); //ouvrir le fichier de config
	if (!configFile.is_open())
	{
		std::cerr << "Error opening config file " << path << std::endl;
		return std::vector<ServerConfig>();
	} 

	std::string content((std::istreambuf_iterator<char>(configFile)), std::istreambuf_iterator<char>()); //lire le contenu du fichier de config
	if (content.empty())
	{
		std::cerr << "config file is empty" << std::endl;
		return std::vector<ServerConfig>();
	}

	std::istringstream stream(content);
	std::string line;
	std::vector<ServerConfig> serverlist;

	while (std::getline(stream, line))
	{
		if (line.find("server") != std::string::npos)
		{
			ServerConfig server;
			if (!parseServer(stream, server))
				return std::vector<ServerConfig>();
			serverlist.push_back(server);
		}
	}
	return serverlist;
}

//A FAIRE : ajouter des verifications d'erreur pour chaque parseur de serveur
//(ex: verifier que le port est un nombre valide, que la taille maximale du corps de la requete est un nombre valide, etc...)


/*

c++ -Wall -Wextra -Werror -std=c++98 main.cpp ParseConfig.cpp ParseServer.cpp ParseLocation.cpp -o webserv

*/