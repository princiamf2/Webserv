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

void Server::addClient(int fd)
{
	Client c;
	c.fd = fd;
	c.toClose = false;
	_clients[fd] = c;
}

void Server::removeClient(int fd)
{
	_clients.erase(fd);
}

void Server::readClient(int fd)
{
	char	buf[4096];
	ssize_t bytes = recv(fd, buf, sizeof(buf), 0);

	if (bytes <= 0) // 0 = déconnexion, -1 = erreur
	{
		_clients[fd].toClose = true; // TODO to handle
		return ;
	}

	_clients[fd].readBuf.append(buf, bytes);

	if (_clients[fd].readBuf.find("\r\n\r\n") != std::string::npos)
	{
		std::string response = handleRawHttpRequest(_clients[fd].readBuf, _conf);
	//		"HTTP/1.1 200 OK\r\n"
	//		"Content-Length: 13\r\n"
	//		"Content-Type: text/plain\r\n"
	//		"\r\n"
	//		"Hello World!\n";
		_clients[fd].writeBuf = response;
		_clients[fd].readBuf.clear();
	}
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
}

bool Server::clientHasData(int fd)
{
	return (!_clients[fd].writeBuf.empty());
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

bool Server::clientToClose(int fd)
{
	return (_clients[fd].toClose);
}

void Server::debug()
{
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

	std::cout << "  locations		: " << _locations.size() << " location(s)" << std::endl;

	std::cout << "  clients		  : " << _clients.size() << " client(s)" << std::endl;
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		std::cout << "	fd=" << it->first << std::endl;
}



