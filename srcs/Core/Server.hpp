#pragma once

//====================(INCLUDES)============================//
#include <iostream>
#include <sys/socket.h>
#include "../HTTPRequest/CgiProcess.hpp"
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
#include "../HTTPRequest/HttpParser.hpp"
#include "../HTTPRequest/HttpResponse.hpp"
#include "../HTTPRequest/RequestHandler.hpp"
#include "../HTTPRequest/CgiManager.hpp"
#include "../HTTPRequest/RequestUtils.hpp"
#include "../HTTPRequest/HttpResponseBuilder.hpp"



//====================(DEFINES)=============================//
# define SUCCESS 1
# define FAIL 0
# define TIMEOUT 60  // general timeout
# define BODYTO 10   // waiting for body timeout

//====================(STRUCTS)=============================//
// client
struct Client {
	int         fd;
	bool        toClose;       // does this client need to be closed
	std::string readBuf;       // data we recieve
	std::string writeBuf;      // data we need to send
	time_t      lastActivity;  // last time the client were active, for incomplete requests...
	bool        waitingBody;   // are we waiting for the rest of the body ?
	bool		cgiActive;     // is a cgi active ?
	CgiProcess	cgi;           // cgi struct
};

//====================(DECLARATIONS)========================//

class Core;

class Server
{
	private:
		ServerConfig                _conf;                                                    // base config (principally for http)
		std::string                 _host;                                                    // like "0.0.0.0"
		std::set<unsigned int>      _ports;                                                   // like 8080
		std::set<std::string>       _domainNames;                                             // like hello.ch
		std::string                 _root;                                                    // like ~/Webserv/srcs/
		std::string                 _index;                                                   // like ~/Webserv/srcs/index.html
		std::map<int, std::string>  _errorPages;                                              // like ~/Webserv/srcs/404.html
		unsigned int                _clientMaxBodySize;                                       // like 100 for 100 Mo
		std::vector<Location>       _locations;                                               // ikd ?
		std::vector<int>            _listenFds;                                               // fds lisned to
		std::map<int, Client>       _clients;                                                 // fds -> client they correspond
		bool                        _autoindex;                                               // page autoindex or not

		bool                        startCgiForClient(int fd, ActionRequest const& action);   // init and start the cgi requested

	public:
		Server(ServerConfig serv);                                    // base constructor
		Server(const Server& other);                                  // base copy constructor
		Server& operator=(const Server& other);                       // base copy assignation constructor
		~Server();                                                    // base destructor

		int         init(void);                                       // init of the binds listen etc
		void        addClient(int fd);                                // add a client to fds list
		void        removeClient(int fd);
		void        readClient(int fd, Core *core);                   // read what client sent
		void        writeClient(int fd);                              // write to the client
		void        closeClient(int fd);                              // close connection to client

		std::vector<int>& getListenFds(void);                         // getter for fds
		std::map<int, Client>&     getClients(void);                  // getter for clients
		bool        clientToClose(int fd);                            // getter pour les toClose
		bool clientWaitingBody(int fd);                               // getter for the waitingBody bool
		bool        clientTimedOut(int fd, time_t now, int timeout);  // getter for the waitingBody bool
		bool        clientHasData(int fd);                            // does a responce for the client exist or is a cgi active

		void finalizeCgi(int clientFd);                               // finalize cgi process and write response
		void debug();                                                 // show server informations
};

//============(UTILS)====================//
//error
int error(std::string s);   // print a message in red and return FAIL;

//readline
int readCommand(Core* C);   // read terminal command and exec them if they correspond to one

//welcome
void welcome();             //print a welcome message
