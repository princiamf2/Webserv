/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ParseConfig.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: marvin <marvin@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/17 20:28:49 by marvin            #+#    #+#             */
/*   Updated: 2026/03/17 20:28:49 by marvin           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

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
	while (std::getline(stream, line)) //lire line by line
	{
		// A FAIRE: analyser chaque ligne et remplir les ServerConfig
		if (line.find("server") != std::string::npos)
		{
		//A FAIRE : creer un nouveau ServerConfig et l'ajouter au vector de ServerConfig
		ServerConfig server;	
		serverlist.push_back(server);
		}
		std::cout << "Line: " << line << std::endl; //pour debug
	}

	return serverlist;
}


