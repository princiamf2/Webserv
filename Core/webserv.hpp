#pragma once

//====================(INCLUDES)============================//
#include <iostream>
#include <sys/socket.h>
#include <poll.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <cstring>

#include "colors.hpp"
#include "../Parsing/Location.hpp"
#include "../Parsing/ParseConfig.hpp"


//====================(DEFINES)=============================//
# define SUCCESS 1
# define FAIL 0

//====================(STRUCTS)=============================//
// client
struct Client {
	int		 fd;
	std::string read_buf;   // recieve
	std::string write_buf;  // to send
	// others : adresse IP, config serveur associée, etc
};

//====================(DECLARATIONS)========================//

class Server
{
	private:
		std::string	 _host; 
		std::set<unsigned int> _ports; //ports d'ecoute
		std::set<std::string> _domainNames; //noms de domaine
		std::string _root; //chemin dossier racine
		std::string _index; //fichier index par defaut
		std::map<int, std::string> _errorPages; //code d'erreur + chemin (page 404 par exemple)
		unsigned int _clientMaxBodySize; //taille max du corps de la requete
		std::vector <Location> _locations; //liste des locations pour ce serveur
		std::vector<struct pollfd> _pollFds; // struct de poll
		std::vector<int> _listenFds; // un fd par port après socket()+bind()+listen()
		bool _autoindex; // false par défaut

	public:
		Server(ServerConfig serv);
		~Server();
		int start(void);
		int loop(void);
};


//============(PRINCIPAL)================//
//main loop
int loop(std::vector<ServerConfig> servers);
int poll_loop();

//============(UTILS)====================//

//clear

//debug

