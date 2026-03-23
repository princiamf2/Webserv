#include "./Server.hpp"

Server::Server(ServerConfig serv)
{
	_host = "127.0.0.1";
	_ports = serv.listen_ports; //ports d'ecoute
	_domainNames = serv.domain_names; //noms de domaine
	_root = serv.root; //chemin dossier racine
	_index = serv.index; //fichier index par defaut
	_errorPages = serv.error_pages; //code d'erreur + chemin (page 404 par exemple)
	_clientMaxBodySize = serv.client_max_body_size; //taille max du corps de la requete
	_locations = serv.locations; //liste des locations pour ce serveur
	// _listenFds; // un fd par port après socket()+bind()+listen(), remplis par init
	_autoindex = false;
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
		addr.sin_family	  = AF_INET;
		addr.sin_port		= htons(*it);
		addr.sin_addr.s_addr = inet_addr(_host.c_str());

		if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
			return (error("Bind returned -1"));
		if (listen(fd, SOMAXCONN) == -1)
			return (error("Listen returned -1"));

		_listenFds.push_back(fd);
	}
	return (SUCCESS);
}

Server::~Server()
{
	//deletes?
}

std::vector<int>& Server::getListenFds(void)
{
	return (_listenFds);
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












/*
int Server::loop(void)
{
	while (true)
	{
		int nReady = poll(_pollFds.data(), _pollFds.size(), -1); // wait for events
		if (nReady < 0)
		{
			if (errno == EINTR)
				continue;
			throw std::runtime_error("poll() failed");
		}
		size_t size = _pollFds.size();

		for (size_t i = 0; i < size; i++)
		{
			if (_pollFds[i].revents == 0)
				continue; // Rien sur le fd
			int fd = _pollFds[i].fd;
			// ── Cas 1 : Erreur ou déconnexion ─────────────────────────
			if (_pollFds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				// close_client(fd);
				;
				continue;
			}
			// ── Cas 2 : Socket d'écoute → nouvelle connexion ──────────
			// if (is_listen_fd(fd) && (_pollFds[i].revents & POLLIN))
			// {
				// accept_new_client(fd);
				// continue;
			// }
			// ── Cas 3 : Socket client → lecture ───────────────────────
			if (_pollFds[i].revents & POLLIN)
			{
				// read_from_client(fd);
				;
			}

			// ── Cas 4 : Socket client → écriture ──────────────────────
			if (_pollFds[i].revents & POLLOUT)
			{
				// write_to_client(fd);
				;
			}
			(void)fd;
		}

		// ─── Phase 3 : Nettoyage des fds marqués ───────────────────────
		// cleanup_closed_fds();

	}
	return (SUCCESS);
}

*/
