#include "Core.hpp"
#include "Server.hpp"
#include "webserv.hpp"
#include <csignal>
#include <sys/poll.h>

Core::Core(std::vector<ServerConfig> configs)
{
	_servers.reserve(configs.size());
	for (size_t i = 0; i < configs.size(); i++)
	{
		_servers.push_back(configs[i]);
		if (_servers[i].init() != SUCCESS)
			throw std::runtime_error("Server init failed");
		addFdsToCore(i);
	}
}

Core::Core(const Core& other)
{
	(void)other;
}

void Core::operator=(const Core& other)
{
	if (this == &other)
		return ;

	return ;
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
	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
	struct pollfd stdinPfd;
	stdinPfd.fd = STDIN_FILENO;
	stdinPfd.events = POLLIN;
	stdinPfd.revents = 0;
	bool quit = 0;

	_pollFds.insert(_pollFds.begin(), stdinPfd); //pour le timeout et cmds bash
	while (true)
	{
		if (quit == true)
			break;
		time_t now = time(NULL);
		std::vector<int> toClose;
		for (std::map<int, Server*>::iterator it = _fdClientToServer.begin(); it != _fdClientToServer.end(); ++it)
		{
			int clientFd = it->first;
			Server* srv = it->second;

			int timeout = srv->clientWaitingBody(clientFd) ? BODYTO : TIMEOUT;
			if (srv->clientTimedOut(clientFd, now, timeout))
			{
				logs("408 timeout fd=" + toString(clientFd));
				Client& client = srv->getClients()[clientFd];
				client.writeBuf = HttpResponseBuilder::buildResponse(
					buildErrorResponse(srv->getConf(), 408, "request timeout"));
				enableClientWrite(clientFd);
			}

			Client& client = srv->getClients()[clientFd];
			if (client.cgiActive)
				CgiManager::checkChild(client.cgi);
			if (client.cgiActive && client.cgi.startTime > 0
				&& now - client.cgi.startTime > CGI_TIMEOUT)
			{
				client.cgi.error = true;
				client.cgi.timedOut = true; // timeout pour finalizeCgi (504)
				logs("cgi timeout fd=" + toString(clientFd) + " pid=" + toString((int)client.cgi.pid));
				if (client.cgi.pid > 0 && !client.cgi.childExited)
					kill(client.cgi.pid, SIGKILL);
				if (client.cgi.stdinFd != -1)
				{
					_cgiWriteFdToClient.erase(client.cgi.stdinFd);
					removePollFd(client.cgi.stdinFd);
				}
				if (client.cgi.stdoutFd != -1)
				{
					_cgiReadFdToClient.erase(client.cgi.stdoutFd);
					removePollFd(client.cgi.stdoutFd);
				}
				srv->finalizeCgi(clientFd);
			}
		}

		for (size_t i = 0; i < toClose.size(); i++)
			closeClient(toClose[i]);

		int ret = poll(_pollFds.data(), _pollFds.size(), 1);

		//poll retour : -1=error, 0=timeout, >0=ready FDs
		if (ret == -1)
		{
			if (errno == EINTR)
				continue;
			error("Poll returned -1");
			break;
		}
		if (ret == 0)
			continue;
		for (size_t i = 0; i <  _pollFds.size(); i++)
		{
			if (i == 0 && (_pollFds[i].revents & POLLIN))
			{
				quit = readCommand(this);
				continue;
			}
			if (_pollFds[i].revents == 0)
				continue; // empty fd

			int fd = _pollFds[i].fd;

			if (_pollFds[i].revents & (POLLERR |  POLLNVAL))
			{
				if (_cgiReadFdToClient.count(fd))
				{
					int clientFd = _cgiReadFdToClient[fd];
					if (!CgiManager::readOutput(_fdClientToServer[clientFd]->getClients()[clientFd].cgi))
					{
						_fdClientToServer[clientFd]->finalizeCgi(clientFd);
						_cgiReadFdToClient.erase(fd);
						_pollFds.erase(_pollFds.begin() + i);
						i--;
					}
				}
				else if (_cgiWriteFdToClient.count(fd))
				{
					_cgiWriteFdToClient.erase(fd);
					_pollFds.erase(_pollFds.begin() + i);
				}
				else
					closeClient(fd);
				i--;
				continue;
			}
			if (_fdToServer.count(fd) && (_pollFds[i].revents & POLLIN)) // new connection
			{
				acceptClient(fd);
				continue;
			}

			if (_fdClientToServer.count(fd)) // a client is actif
			{
				if (_pollFds[i].revents & POLLIN)
					_fdClientToServer[fd]->readClient(fd, this);

				if (_fdClientToServer.count(fd) && (_pollFds[i].revents & POLLOUT))
					_fdClientToServer[fd]->writeClient(fd);
			}
			if (_fdClientToServer.count(fd))
			{
				_pollFds[i].events = POLLIN;
				if (_fdClientToServer[fd]->clientHasData(fd))
					_pollFds[i].events |= POLLOUT;
			}

			if (_fdClientToServer.count(fd) && _fdClientToServer[fd]->clientToClose(fd))
			{
				closeClient(fd);
				i--;
				continue;
			}

			// CGI stdin ready to write
			if (_cgiWriteFdToClient.count(fd) && (_pollFds[i].revents & POLLOUT))
			{
				int clientFd = _cgiWriteFdToClient[fd];

				if (!CgiManager::writeInput(_fdClientToServer[clientFd]->getClients()[clientFd].cgi))
				{
					_fdClientToServer[clientFd]->finalizeCgi(clientFd);
					_cgiWriteFdToClient.erase(fd);
					_pollFds.erase(_pollFds.begin() + i);
					i--;
					continue;
				}

				// if stdin closed -> remove from _pollFds
				if (_fdClientToServer[clientFd]->getClients()[clientFd].cgi.stdinClosed)
				{
					_cgiWriteFdToClient.erase(fd);
					_pollFds.erase(_pollFds.begin() + i);
					i--;
				}
			}

			// CGI stdout ready to read
			if (_cgiReadFdToClient.count(fd) && (_pollFds[i].revents & POLLIN))
			{
				int clientFd = _cgiReadFdToClient[fd];
				if (!CgiManager::readOutput(_fdClientToServer[clientFd]->getClients()[clientFd].cgi))
				{
					_fdClientToServer[clientFd]->finalizeCgi(clientFd);
					_cgiReadFdToClient.erase(fd);
					_pollFds.erase(_pollFds.begin() + i);
					i--;
					continue;
				}
			}

			// CGI stdout closed -> CGI did end
			if (_cgiReadFdToClient.count(fd) && (_pollFds[i].revents & POLLHUP))
			{
				int clientFd = _cgiReadFdToClient[fd];
				CgiProcess& cgi = _fdClientToServer[clientFd]->getClients()[clientFd].cgi;

				if (!CgiManager::readOutput(cgi))
				{
					_fdClientToServer[clientFd]->finalizeCgi(clientFd);
					_cgiReadFdToClient.erase(fd);
					_pollFds.erase(_pollFds.begin() + i);
					i--;
					continue;
				}

				if (cgi.stdoutClosed)
				{
					_fdClientToServer[clientFd]->finalizeCgi(clientFd);
					_cgiReadFdToClient.erase(fd);
					_pollFds.erase(_pollFds.begin() + i);
					i--;
					continue;
				}
			}

		}
	}

	for (size_t i = 0; i < _pollFds.size(); i++)
		close(_pollFds[i].fd);
	_pollFds.clear();
	_fdClientToServer.clear();
	_fdToServer.clear();
}

std::vector<Server>& Core::getServers()
{
	return (_servers);
}

void Core::acceptClient(int listenFd)
{
	struct sockaddr_in clientAddr;
	socklen_t clientLen = sizeof(clientAddr);

	int clientFd = accept(listenFd, (struct sockaddr*)&clientAddr, &clientLen);
	if (clientFd == -1)
		return ((void)error("Accept returned -1"));
	fcntl(clientFd, F_SETFL, fcntl(clientFd, F_GETFL, 0) | O_NONBLOCK);
	logs("accept client fd=" + toString(clientFd) + " listen fd=" + toString(listenFd));

	struct pollfd pfd = {clientFd, POLLIN, 0};
	_pollFds.push_back(pfd);

	_fdClientToServer[clientFd] = _fdToServer[listenFd]; // associate client to server
	_fdToServer[listenFd]->addClient(clientFd); // add client in server
}

void Core::removePollFd(int fd) 
{
	for (size_t i = 0; i < _pollFds.size(); ++i)
	{
		if (_pollFds[i].fd == fd)
		{
			_pollFds.erase(_pollFds.begin() + i);
			return ;
		}
	}
}

void Core::enableClientWrite(int fd) // petite boucle pour activer le write d'un client dans poll (pour le timeout ou quand la reponse est prete)
{
	for (size_t i = 0; i < _pollFds.size(); ++i)
	{
		if (_pollFds[i].fd == fd)
		{
			_pollFds[i].events |= POLLOUT;
			return ;
		}
	}
}

void Core::closeClient(int fd)
{
	if (_fdClientToServer.count(fd))
	{
		Server *srv = _fdClientToServer[fd];
		Client &client = srv->getClients()[fd];
		logs("close client fd=" + toString(fd));

		if (client.cgiActive)
		{
			if (client.cgi.pid > 0 && !client.cgi.childExited)
				kill(client.cgi.pid, SIGKILL);

			if (client.cgi.stdinFd != -1)
			{
				_cgiWriteFdToClient.erase(client.cgi.stdinFd);
				removePollFd(client.cgi.stdinFd);
			}
			if (client.cgi.stdoutFd != -1)
			{
				_cgiReadFdToClient.erase(client.cgi.stdoutFd);
				removePollFd(client.cgi.stdoutFd);
			}
			CgiManager::cleanupProcess(client.cgi);
			client.cgiActive = false;
		}

		srv->removeClient(fd);
		_fdClientToServer.erase(fd);
	}

	removePollFd(fd);
	close(fd);
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

	std::cout << std::endl << "--- _fdClientToServer ---" << std::endl;
	for (std::map<int, Server*>::iterator it = _fdClientToServer.begin(); it != _fdClientToServer.end(); ++it)
		std::cout << "  fd=" << it->first << " -> Server@" << it->second << std::endl;

	std::cout << std::endl << CYAN << "========== SERVERS DEBUG ==========" << RESET << std::endl;
	for (size_t i = 0; i < _servers.size(); i++)
	{
		std::cout << std::endl << "[Server " << i << "] @ " << &_servers[i] << std::endl;
		_servers[i].debug();
	}
	std::cout << "====================================" << std::endl << std::endl;
}
