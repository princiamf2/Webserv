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
#include "./Server.hpp"

//====================(DEFINES)=============================//
# define SUCCESS 1
# define FAIL 0

//====================(STRUCTS)=============================//


//====================(DECLARATIONS)========================//

class Core
{
	private:
		std::vector<struct pollfd>  _pollFds;    // all fds lisned to
		std::vector<Server>         _servers;    // all servers
		std::map<int, Server*>      _fdToServer; // fd lisned -> server they correspond
		std::map<int, Server*>      _fdToClient; // client fd -> server they correspond

	public:
		Core(std::vector<ServerConfig> configs);
		~Core();
		void addFdsToCore(size_t serverIndex);
		int  init(void);                         // init of the binds listen etc
		void runPoll();                          // run main loop with poll
		void acceptClient(int listenFd);         // add a client to fds list
		void closeClient(int fd);           // close and remove client
		void debug();
};

