#include "Core.hpp"

Core::Core(std::vector<ServerConfig> configs)
{
	_servers.reserve(configs.size());
	for (size_t i = 0; i < configs.size(); i++)
	{
		Server s(configs[i]);
		s.init();
		_servers.push_back(s);
		addFdsToCore(i);
	}
}

void Core::addFdsToCore(size_t serverIndex)
{
	Server* srv = &_servers[serverIndex];
	std::vector<int>& listenFds = srv->getListenFds();

	for (size_t i = 0; i < listenFds.size(); i++)
	{
		struct pollfd pfd;
		pfd.fd	  = listenFds[i];
		pfd.events  = POLLIN;
		pfd.revents = 0;
		_pollFds.push_back(pfd);
		_fdToServer[listenFds[i]] = srv; // set fd -> server
	}
}

Core::~Core() {};

void Core::debug()
{
    std::cout << BLUE << "========== CORE DEBUG ==========" << RESET << std::endl;
    std::cout << "Total servers  : " << _servers.size() << std::endl;
    std::cout << "Total pollFds  : " << _pollFds.size() << std::endl;

    std::cout << std::endl << "--- _pollFds ---" << std::endl;
    for (size_t i = 0; i < _pollFds.size(); i++)
        std::cout << "  [" << i << "] fd=" << _pollFds[i].fd
                  << " events=" << _pollFds[i].events << std::endl;

    std::cout << std::endl << "--- _fdToServer ---" << std::endl;
    for (std::map<int, Server*>::iterator it = _fdToServer.begin(); it != _fdToServer.end(); ++it)
        std::cout << "  fd=" << it->first << " -> Server@" << it->second << std::endl;

    std::cout << std::endl << "--- _fdToClient ---" << std::endl;
    for (std::map<int, Server*>::iterator it = _fdToClient.begin(); it != _fdToClient.end(); ++it)
        std::cout << "  fd=" << it->first << " -> Server@" << it->second << std::endl;

    std::cout << std::endl << CYAN << "========== SERVERS DEBUG ==========" << RESET << std::endl;
    for (size_t i = 0; i < _servers.size(); i++)
    {
        std::cout << std::endl << "[Server " << i << "] @ " << &_servers[i] << std::endl;
        _servers[i].debug();
    }
    std::cout << "====================================" << std::endl << std::endl;
}
