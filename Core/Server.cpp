#include "./Server.hpp"

Server::Server(ServerConfig serv)
{
	_conf = serv;
	_host = "127.0.0.1";
	_ports = serv.listen_ports; //ports d'ecoute
	_domainNames = serv.domain_names; //noms de domaine
	_root = serv.root; //chemin dossier racine
	_index = serv.index; //fichier index par defaut
	_errorPages = serv.error_pages; //code d'erreur + chemin (page 404 par exemple)
	_clientMaxBodySize = serv.client_max_body_size; //taille max du corps de la requete
	_locations = serv.locations; //liste des locations pour ce serveur
	// _listenFds; // un fd par port apres socket()+bind()+listen(), remplis par init
	_autoindex = false;
}

Server::Server(const Server& other)
	: _conf(other._conf),
	_host(other._host),
	_ports(other._ports),
	_domainNames(other._domainNames),
	_root(other._root),
	_index(other._index),
	_errorPages(other._errorPages),
	_clientMaxBodySize(other._clientMaxBodySize),
	_locations(other._locations),
	_listenFds(other._listenFds),
	_clients(other._clients),
	_autoindex(other._autoindex)
{}

Server& Server::operator=(const Server& other)
{
	if (this == &other)
		return (*this);
	_conf = other._conf;
	_host = other._host;
	_ports = other._ports;
	_domainNames = other._domainNames;
	_root = other._root;
	_index = other._index;
	_errorPages = other._errorPages;
	_clientMaxBodySize = other._clientMaxBodySize;
	_locations = other._locations;
	_autoindex = other._autoindex;
	_listenFds = other._listenFds;
	_clients = other._clients;
	return (*this);
}

int Server::init(void)
{
	for (std::set<unsigned int>::iterator it = _ports.begin(); it != _ports.end(); ++it)
	{
		int fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd == -1)
			return (error("Socket returned -1"));
		int opt = 1;
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(*it);
		addr.sin_addr.s_addr = inet_addr(_host.c_str());

		if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
			return (error("Bind returned -1"));
		if (listen(fd, SOMAXCONN) == -1)
			return (error("Listen returned -1"));

		_listenFds.push_back(fd);
	}
	return (SUCCESS);
}

void Server::finalizeCgi(int clientFd)
{
	CgiManager::checkChild(_clients[clientFd].cgi);
	CgiResult result = CgiManager::buildFinalResult(_clients[clientFd].cgi);
	CgiManager::cleanupProcess(_clients[clientFd].cgi);
	_clients[clientFd].cgiActive = false;
	std::cout << result.rawOutput << std::endl;
	if (!result.success)
		_clients[clientFd].writeBuf = HttpResponseBuilder::buildResponse(
			buildErrorResponse(_conf, 500, "cgi failed"));
	else
		_clients[clientFd].writeBuf = HttpResponseBuilder::buildResponse(result.response);
}

bool Server::clientWaitingBody(int fd)
{
	return (_clients[fd].waitingBody);
}
void Server::addClient(int fd)
{
	Client c;
	c.fd = fd;
	c.toClose = false;
	c.waitingBody = false;
	c.cgiActive = false;
	c.lastActivity = time(NULL);
	_clients[fd] = c;
}

void Server::removeClient(int fd)
{
	_clients.erase(fd);
}

bool Server::clientTimedOut(int fd, time_t now, int timeout)
{
	return (now - _clients[fd].lastActivity > timeout);
}

bool Server::startCgiForClient (int fd, ActionRequest const& action)
{
	Client& client = _clients[fd];
	if (!CgiManager::startProcess(client.cgi, action.request, _conf, action.location, action.scriptPath, action.interpreter))
		return false;
	client.cgiActive = true;
	return true;
}
#include "Core.hpp"
void Server::readClient(int fd, Core *core)
{
	char	buf[4096];
	ssize_t bytes = recv(fd, buf, sizeof(buf), 0);

	if (bytes <= 0) // 0 = déconnexion, -1 = erreur
	{
		if (_clients[fd].waitingBody)
		{
			_clients[fd].writeBuf = HttpResponseBuilder::buildResponse(buildErrorResponse(_conf, 400, "incomplete request body"));
			_clients[fd].waitingBody = false;
		}
		else
			_clients[fd].toClose = true;
		return ;
	}
	_clients[fd].lastActivity = time(NULL);

	_clients[fd].readBuf.append(buf, bytes);

	size_t headerEnd = _clients[fd].readBuf.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
		return ; // wiating for headers to end

	size_t contentLength = 0;
	size_t clPos = _clients[fd].readBuf.find("Content-Length: ");

	if (clPos != std::string::npos && clPos < headerEnd)
	{
		size_t clEnd = _clients[fd].readBuf.find("\r\n", clPos);
		std::string tmp = _clients[fd].readBuf.substr(clPos + 16, clEnd - clPos - 16);
		contentLength = std::atoll(tmp.c_str());
		std::cout << "lenght found, pos: " << clPos << ", lenght: " << contentLength << std::endl;
		size_t bodySize = _clients[fd].readBuf.size() - (headerEnd + 4);
		if (bodySize < contentLength)
		{
			_clients[fd].waitingBody = true;
			return ; // incomplete body
		}
	}
	_clients[fd].waitingBody = false;
	try
	{
		HttpRequest request = HttpParser::parseRequest(_clients[fd].readBuf);
		Location const* location = findBestLocation(_conf, request.path);
		ActionRequest action = RequestHandler::resolveAction(request, _conf, location);

		if (action.type == ACTION_START_CGI)
		{
			if (!startCgiForClient(fd, action))
				_clients[fd].writeBuf = HttpResponseBuilder::buildResponse(
					buildErrorResponse(_conf, 500, action.scriptPath));
			else
				core->registerCgi(_clients[fd].fd, _clients[fd].cgi.stdinFd, _clients[fd].cgi.stdoutFd);
		}
		else
			_clients[fd].writeBuf = HttpResponseBuilder::buildResponse(action.response);
	}
	catch (std::exception const& e)
	{
		(void)e;
		_clients[fd].writeBuf = HttpResponseBuilder::buildResponse(
			buildErrorResponse(_conf, 400, "bad request"));
	}
	_clients[fd].readBuf.clear();
	return ;

}

void Server::writeClient(int fd)
{
	if (_clients[fd].writeBuf.empty())
		return ;

	ssize_t bytes = send(fd, _clients[fd].writeBuf.c_str(),
							 _clients[fd].writeBuf.size(), 0);
	if (bytes == -1)
	{
		_clients[fd].toClose = true;
		return ;
	}
	_clients[fd].writeBuf.erase(0, bytes); // remove only added bytes
	if (_clients[fd].writeBuf.empty())
		_clients[fd].toClose = true;
	_clients[fd].lastActivity = time(NULL);
}

bool Server::clientHasData(int fd)
{
	return (!_clients[fd].writeBuf.empty() || _clients[fd].cgiActive);
}

Server::~Server()
{
//	for (size_t i = 0; i < _listenFds.size(); i++)
//		close(_listenFds[i]); // close listen socket
//	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
//		close(it->first); // close clients
}

std::vector<int>& Server::getListenFds(void)
{
	return (_listenFds);
}

std::map<int, Client>& Server::getClients(void)
{
	return (_clients);
}

bool Server::clientToClose(int fd)
{
	return (_clients[fd].toClose);
}

void Server::debug()
{
	int n = 0;
	std::cout << "  host			 : " << _host << std::endl;
	std::cout << "  root			 : " << _root << std::endl;
	std::cout << "  index			: " << _index << std::endl;
	std::cout << "  autoindex		: " << (_autoindex ? "on" : "off") << std::endl;
	std::cout << "  clientMaxBody	: " << _clientMaxBodySize << std::endl;

	std::cout << "  ports			: ";
	for (std::set<unsigned int>::iterator it = _ports.begin(); it != _ports.end(); ++it)
		std::cout << *it << " ";
	std::cout << std::endl;

	std::cout << "  domainNames	  : ";
	for (std::set<std::string>::iterator it = _domainNames.begin(); it != _domainNames.end(); ++it)
		std::cout << *it << " ";
	std::cout << std::endl;

	std::cout << "  listenFds		: ";
	for (size_t i = 0; i < _listenFds.size(); i++)
		std::cout << _listenFds[i] << " ";
	std::cout << std::endl;

	std::cout << "  errorPages	   :" << std::endl;
	for (std::map<int, std::string>::iterator it = _errorPages.begin(); it != _errorPages.end(); ++it)
		std::cout << "	" << it->first << " -> " << it->second << std::endl;

	std::cout << "  locations        : " << _locations.size() << " location(s)" << std::endl;
	
	for (std::vector<Location>::iterator it = _locations.begin(); it != _locations.end(); ++it)
	{
		n++;
	    std::cout << "	" << n << std::endl;
	    std::cout << "		path: -> " << it->path << std::endl;
	
	    // afficher les méthodes autorisées
	    std::cout << "		methods: " << std::endl;
	    for (std::set<std::string>::iterator it2 = it->allowed_methods_http.begin();
	         it2 != it->allowed_methods_http.end(); ++it2)
	        std::cout << "			- " << *it2 << std::endl;
	}

	std::cout << "  clients		  : " << _clients.size() << " client(s)" << std::endl;
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		std::cout << "	fd=" << it->first << std::endl;
}



