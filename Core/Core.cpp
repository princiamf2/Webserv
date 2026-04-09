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

void Core::registerCgi(int clientFd, int stdinFd, int stdoutFd)
{
	if (stdinFd != -1)
	{
		struct pollfd pfd = {stdinFd, POLLOUT, 0};
		_pollFds.push_back(pfd);
		_cgiWriteFdToClient[stdinFd] = clientFd;
	}
	if (stdoutFd != -1)
	{
		struct pollfd pfd = {stdoutFd, POLLIN, 0};
		_pollFds.push_back(pfd);
		_cgiReadFdToClient[stdoutFd] = clientFd;
	}
}

void Core::runPoll()
{
	while (true) //we'll need to handle signals
	{
		time_t now = time(NULL);
		std::vector<int> toClose;
		for (std::map<int, Server*>::iterator it = _fdToClient.begin(); it != _fdToClient.end(); ++it)
		{
			int clientFd = it->first;
			Server* srv = it->second;
		
			int timeout = srv->clientWaitingBody(clientFd) ? BODYTO : TIMEOUT;
			if (srv->clientTimedOut(clientFd, now, timeout))
				toClose.push_back(clientFd);
		}

		for (size_t i = 0; i < toClose.size(); i++)
			closeClient(toClose[i]);



		int ret = poll(_pollFds.data(), _pollFds.size(), 1);

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
				closeClient(fd);
				size--;
				i--;
				continue;
			}

			if ((_pollFds[i].events & POLLHUP) && _fdToClient.count(fd))
			{
				closeClient(fd);
				size--;
				i--;
				continue;
			}
			if (_fdToServer.count(fd) && (_pollFds[i].revents & POLLIN)) // new connection
			{
				acceptClient(fd);
				continue;
			}
			if (_fdToClient.count(fd)) // a client is actif
			{
				if (_pollFds[i].revents & POLLIN)
					_fdToClient[fd]->readClient(fd);
				if (_fdToClient.count(fd) && (_pollFds[i].revents & POLLOUT))
					_fdToClient[fd]->writeClient(fd);
			}
			if (_fdToClient.count(fd))
			{
				_pollFds[i].events = POLLIN;
				if (_fdToClient[fd]->clientHasData(fd))
					_pollFds[i].events |= POLLOUT;
			}

			if (_fdToClient.count(fd) && _fdToClient[fd]->clientToClose(fd))
			{
				closeClient(fd);
				size--;
				i--;
			}









			// Pipe stdin CGI prêt en écriture
			if (_cgiWriteFdToClient.count(fd) && (_pollFds[i].revents & POLLOUT))
			{
				int clientFd = _cgiWriteFdToClient[fd];

				CgiManager::writeInput(_fdToClient[clientFd]->getClients()[clientFd].cgi);

				// Si stdin fermé -> retirer de _pollFds
				if (_fdToClient[clientFd]->getClients()[clientFd].cgi.stdinClosed)
				{
					_cgiWriteFdToClient.erase(fd);
					_pollFds.erase(_pollFds.begin() + i);
					size--;
					i--;
				}
			}
			
			// Pipe stdout CGI prêt en lecture
			if (_cgiReadFdToClient.count(fd) && (_pollFds[i].revents & POLLIN))
			{
				int clientFd = _cgiReadFdToClient[fd];
				CgiManager::readOutput(_fdToClient[clientFd]->getClients()[clientFd].cgi);
			}
			
			// Pipe stdout CGI fermé -> CGI terminé
			if (_cgiReadFdToClient.count(fd) && (_pollFds[i].revents & POLLHUP))
			{
				int clientFd = _cgiReadFdToClient[fd];
				_fdToClient[clientFd]->finalizeCgi(clientFd);
				_cgiReadFdToClient.erase(fd);
				_pollFds.erase(_pollFds.begin() + i);
				size--;
				i--;
			}
		}
	}
}

void Core::acceptClient(int listenFd)
{
	struct sockaddr_in clientAddr;
	socklen_t clientLen = sizeof(clientAddr);

	int clientFd = accept(listenFd, (struct sockaddr*)&clientAddr, &clientLen);
	if (clientFd == -1)
		return ((void)error("Accept returned -1"));
	fcntl(clientFd, F_SETFL, fcntl(clientFd, F_GETFL, 0) | O_NONBLOCK);

	struct pollfd pfd = {clientFd, POLLIN, 0};
	_pollFds.push_back(pfd);

	_fdToClient[clientFd] = _fdToServer[listenFd]; // associate client to server
	_fdToServer[listenFd]->addClient(clientFd); // add client in server
	std::cout << MAGENTA << "CLIENT ADDED" << RESET << std::endl;
}

void Core::closeClient(int fd)
{
	if (_fdToClient.count(fd))
	{
		_fdToClient[fd]->removeClient(fd);
		_fdToClient.erase(fd);
	}

	for (size_t i = 0; i < _pollFds.size(); i++)
	{
		if (_pollFds[i].fd == fd)
		{
			_pollFds.erase(_pollFds.begin() + i);
			break ;
		}
	}
	close(fd);
	std::cout << MAGENTA << "CLIENT REMOVED" << RESET << std::endl;
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
