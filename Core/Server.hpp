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
#include <unistd.h>

#include "colors.hpp"
#include "../Parsing/Location.hpp"
#include "../Parsing/ParseConfig.hpp"
#include "../HTTPRequest/HttpModule.hpp"


//====================(DEFINES)=============================//
# define SUCCESS 1
# define FAIL 0

//====================(STRUCTS)=============================//
// client
struct Client {
	int         fd;
	bool        toClose;
	std::string readBuf;   // recieve
	std::string writeBuf;  // to send
	// others : adresse IP, config serveur associée, etc
};

//====================(DECLARATIONS)========================//

class Server
{
	private:
		ServerConfig                _conf;              // base config (principally for http)
		std::string                 _host;              // like "0.0.0.0"
		std::set<unsigned int>      _ports;             // like 8080
		std::set<std::string>       _domainNames;       // like hello.ch
		std::string                 _root;              // like ~/Webserv/srcs/
		std::string                 _index;             // like ~/Webserv/srcs/index.html
		std::map<int, std::string>  _errorPages;        // like ~/Webserv/srcs/404.html
		unsigned int                _clientMaxBodySize; // like 100 for 100 Mo
		std::vector<Location>       _locations;         // ikd ?
		std::vector<int>            _listenFds;         // fds lisned to
		std::map<int, Client>       _clients;           // fds -> client they correspond
		bool                        _autoindex;         // page autoindex or not

	public:
		Server(ServerConfig serv);
		Server(const Server& other);
		Server& operator=(const Server& other);
		~Server();
		int         init(void);                         // init of the binds listen etc
		void        addClient(int fd);                  // add a client to fds list
		void        removeClient(int fd);
		void        readClient(int fd);                 // read what client sent
		void        writeClient(int fd);                // write to the client
		void        closeClient(int fd);                // close connection to client
		bool        clientHasData(int fd);              // does a responce for the client exist
		std::vector<int>& getListenFds(void);           // getter for fds
		bool        clientToClose(int fd);              // getter pour les toClose
		void debug();
};

//============(UTILS)====================//
//error
int error(std::string s);
