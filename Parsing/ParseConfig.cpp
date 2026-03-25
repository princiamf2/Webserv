
#include "ParseConfig.hpp"
#include <fstream> //permet de lire le fichier de config
#include <iostream>
#include <sstream> //permet de lire le contenu du configfile ligne par ligne


std::string stripSemicolon(const std::string& s) //pour enlever le ; a la fin de chaque ligne
{
	return s.substr(0, s.find(";"));
}

bool expectOpenBracket(std::istringstream& current_line, std::istringstream& stream)
{
    std::string bracket;
    current_line >> bracket;
    if (bracket != "{")
    {
        std::getline(stream, bracket);
        std::istringstream iss(bracket);
        std::string bracket;
        iss >> bracket;
        if (bracket != "{")
        {
            std::cerr << "Error: expected '{'" << std::endl;
            return false;
        }
    }
    return true;
}

std::vector<ServerConfig> parseConfig(std::string path)
{
    std::ifstream configFile(path.c_str());
    if (!configFile.is_open())
    {
        std::cerr << "Error opening config file " << path << std::endl;
        return std::vector<ServerConfig>();
    }
    std::string content((std::istreambuf_iterator<char>(configFile)), std::istreambuf_iterator<char>());
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
        std::istringstream in_ss(line);
        std::string name_to_parse;
        in_ss >> name_to_parse;
		if (name_to_parse.empty() || name_to_parse.substr(0, 2) == "//") //pour les commentaires et les lignes vides
    		continue;
        if (name_to_parse == "server")
        {
            if (!expectOpenBracket(in_ss, stream))
                return std::vector<ServerConfig>();
            ServerConfig server;
            if (!parseServer(stream, server))
                return std::vector<ServerConfig>();
            serverlist.push_back(server);
        }
    }
    return serverlist;
}



/*

c++ -Wall -Wextra -Werror -std=c++98 main.cpp ParseConfig.cpp ParseServer.cpp ParseLocation.cpp -o webserv && ./webserv test.config

*/