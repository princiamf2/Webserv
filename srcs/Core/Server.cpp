#include "./Server.hpp"
#include "Core.hpp"
#include <cctype>

Server::Server(ServerConfig serv)
{
	_conf = serv;                                    // parsing configuration keeped
	_host = "127.0.0.1";                             // host
	_ports = serv.listen_ports;                      // listen ports
	_domainNames = serv.domain_names;                // domain name
	_root = serv.root;                               // root directory path
	_index = serv.index;                             // index file by default
	_errorPages = serv.error_pages;                  // error code -> path (404 page for example)
	_clientMaxBodySize = serv.client_max_body_size;  // max size of request body
	_locations = serv.locations;                     // locations list for this serveur
	// _listenFds;                                   // one fd by port after socket()+bind()+listen(), filled by init
	// _clients;                                     // clients fds -> client they correspond, filled as we go along by Core::acceptClient
	_autoindex = false;                              // autoindex ?
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
		addr.sin_addr.s_addr = INADDR_ANY;

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
	bool wasTimeout = _clients[clientFd].cgi.timedOut;
	CgiManager::cleanupProcess(_clients[clientFd].cgi);
	_clients[clientFd].cgiActive = false;
	if (!result.success)
	{
		int errorCode = wasTimeout ? 504 : 500;
		_clients[clientFd].writeBuf = HttpResponseBuilder::buildResponse(
			buildErrorResponse(_conf, errorCode, errorCode == 504 ? "cgi timeout" : "cgi failed"));
	}
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

static std::string toLowerString(std::string s)
{
	for (size_t i = 0; i < s.size(); i++)
		s[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
	return s;
}

void Server::readClient(int fd, Core *core)
{
	char	buf[4096];
	ssize_t bytes = recv(fd, buf, sizeof(buf), 0);

	if (bytes <= 0) // 0 = deconnexion, -1 = error
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
	std::string headersPart = _clients[fd].readBuf.substr(0, headerEnd);
	std::string headersLower = toLowerString(headersPart);
	size_t clPos = headersLower.find("content-length:");

	if (clPos != std::string::npos && clPos < headerEnd)
	{
		size_t clEnd = headersPart.find("\r\n", clPos);
		std::string tmp = headersPart.substr(clPos + 15, clEnd - clPos - 15);
		contentLength = std::atoll(tmp.c_str());
		if (_clientMaxBodySize > 0 && contentLength > _clientMaxBodySize)
		{
			_clients[fd].writeBuf = HttpResponseBuilder::buildResponse(
				buildErrorResponse(_conf, 413, "payload too large"));
			_clients[fd].waitingBody = false;
			_clients[fd].readBuf.clear();
			return ;
		}
		size_t bodySize = _clients[fd].readBuf.size() - (headerEnd + 4);
		if (_clientMaxBodySize > 0 && bodySize > _clientMaxBodySize)
		{
			_clients[fd].writeBuf = HttpResponseBuilder::buildResponse(
				buildErrorResponse(_conf, 413, "payload too large"));
			_clients[fd].waitingBody = false;
			_clients[fd].readBuf.clear();
			return ;
		}
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
	//send=0 (disconnect), <0 (erreur) -> ferme client
	if (bytes <= 0)
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
}

std::vector<int>& Server::getListenFds(void)
{
	return (_listenFds);
}

std::map<int, Client>& Server::getClients(void)
{
	return (_clients);
}

ServerConfig& Server::getConf(void)
{
	return (_conf);
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

	    // show authoriwed methods
	    std::cout << "		methods: " << std::endl;
	    for (std::set<std::string>::iterator it2 = it->allowed_methods_http.begin();
	         it2 != it->allowed_methods_http.end(); ++it2)
	    std::cout << "			- " << *it2 << std::endl;

		std::cout << "\t\tcgi_extensions: ";
		for (std::set<std::string>::iterator it3 = it->cgi_extensions.begin();
			it3 != it->cgi_extensions.end(); ++it3)
			std::cout << *it3 << " ";
		std::cout << std::endl;

		std::cout << "\t\tcgi_interpreters: ";
		for (std::map<std::string, std::string>::iterator it4 = it->cgi_interpreters.begin();
			it4 != it->cgi_interpreters.end(); ++it4)
			std::cout << "(" << it4->first << " -> " << it4->second << ") ";
		std::cout << std::endl;
	}

	std::cout << "  clients		  : " << _clients.size() << " client(s)" << std::endl;
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		std::cout << "	fd=" << it->first << std::endl;
}



