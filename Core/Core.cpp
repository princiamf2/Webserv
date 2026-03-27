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

void Core::runPoll()
{
<<<<<<< HEAD
    while (true) //we'll need to handle signals
    {
        int ret = poll(_pollFds.data(), _pollFds.size(), -1); // timeout 5s

        if (ret == -1)
        {
            if (errno == EINTR)
                continue;
            error("Poll returned -1");
            break;
        }
        if (ret == 0)
            continue;

        size_t size = _pollFds.size();
        for (size_t i = 0; i < size; i++)
        {
            if (_pollFds[i].revents == 0)
                continue; // empty fd

            int fd = _pollFds[i].fd;

            if (_pollFds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                // closeClient(fd);
                continue;
            }
            if (_fdToServer.count(fd) && (_pollFds[i].revents & POLLIN)) // new connection
            {
                // acceptClient(fd);
                continue;
            }
            if (_fdToClient.count(fd)) // a client is actif
            {
                if (_pollFds[i].revents & POLLIN)
                    // _fdToClient[fd]->readClient(fd);
					std::cout << " _fdToClient[fd]->readClient(fd);" << std::endl;
                if (_pollFds[i].revents & POLLOUT)
                    // _fdToClient[fd]->writeClient(fd);
					std::cout << "_fdToClient[fd]->writeClient(fd);" << std::endl;
            }
        }
    }
=======
	while (true) //we'll need to handle signals
	{
		int ret = poll(_pollFds.data(), _pollFds.size(), -1); // timeout 5s

		if (ret == -1)
		{
			if (errno == EINTR)
				continue;
			error("Poll returned -1");
			break;
		}
		if (ret == 0)
			continue;

		size_t size = _pollFds.size();
		for (size_t i = 0; i < size; i++)
		{
			if (_pollFds[i].revents == 0)
				continue; // empty fd

			int fd = _pollFds[i].fd;

			if (_pollFds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				// closeClient(fd);
				continue;
			}
			if (_fdToServer.count(fd) && (_pollFds[i].revents & POLLIN)) // new connection
			{
				// acceptClient(fd);
				continue;
			}
			if (_fdToClient.count(fd)) // a client is actif
			{
				if (_pollFds[i].revents & POLLIN)
					// _fdToClient[fd]->readClient(fd);
					std::cout << " _fdToClient[fd]->readClient(fd);" << std::endl;
				if (_pollFds[i].revents & POLLOUT)
					// _fdToClient[fd]->writeClient(fd);
					std::cout << "_fdToClient[fd]->writeClient(fd);" << std::endl;
			}
		}
	}
>>>>>>> origin/Core_mae
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
