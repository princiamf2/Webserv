
#include "ParseConfig.hpp"
#include <fstream> //permet de lire le fichier de config
#include <iostream>
#include <sstream> //permet de lire le contenu du configfile ligne par ligne


bool stripSemicolon(std::string const& s, std::string& result)
{
	size_t pos = s.find(';');

	if (pos == std::string::npos)
		return false;
	if (pos != s.size() - 1)
		return false;
	result = s.substr(0, pos);
	return true;
}

// verifie qu'il reste un seul token sur la ligne, pas un mot en plus
static bool extractSingleToken(std::istringstream& lineStream, std::string& token)
{
    std::string extra;

    if (!(lineStream >> token))
        return false;
    if (lineStream >> extra)
        return false;
    return true;
}

// accepte soit "server {" sur une ligne, soit "server" puis "{" sur la ligne suivante
bool expectOpenBracket(std::istringstream& current_line, std::istringstream& stream)
{
    std::string bracket;
    std::string nextLine;
    std::istringstream nextLineStream;

    if (extractSingleToken(current_line, bracket))
    {
        if (bracket == "{")
            return true;
        std::cerr << "Error: expected '{'" << std::endl;
        
        return false;
    }
    if (!std::getline(stream, nextLine))
    {
        std::cerr << "Error: expected '{'" << std::endl;
        return false;
    }
    nextLineStream.str(nextLine);
    if (!extractSingleToken(nextLineStream, bracket) || bracket != "{")
    {
        std::cerr << "Error: expected '{'" << std::endl;
        return false;
    }
    return true;
}

static bool hasListenConflict(const std::vector<ServerConfig>& serverlist, const ServerConfig& candidate)
{
    for (size_t i = 0; i < serverlist.size(); ++i)
    {
        for (std::set<unsigned int>::const_iterator iter = candidate.listen_ports.begin(); iter != candidate.listen_ports.end(); ++iter)
        {
            if (serverlist[i].listen_ports.count(*iter))
            {
                std::cerr << "Error -> multiple servers for same port " << *iter << std::endl;
                return true;
            }
        }
    }
    return false;
}

// lit le fichier bloc par bloc et refuse ce qui n'est pas un vrai debut de server
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
            if (hasListenConflict(serverlist, server))
                return std::vector<ServerConfig>();
            serverlist.push_back(server);
        }
		else
		{
            std::cerr << "ERROR: INVALID TOP-LEVEL TOKEN: " << name_to_parse << std::endl;
			return std::vector<ServerConfig>();
		}
    }
    return serverlist;
}



/*

c++ -Wall -Wextra -Werror -std=c++98 main.cpp ParseConfig.cpp ParseServer.cpp ParseLocation.cpp -o webserv && ./webserv test.config

*/