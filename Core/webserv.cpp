#include "webserv.hpp"

int error(std::string s)
{
	std::cerr << ERROR << s << std::endl;
	perror(s.c_str());
	return (FAIL);
}

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
	// _pollFds; // structs de poll
	// _listenFds; // un fd par port après socket()+bind()+listen()
	_autoindex = false;
}

Server::~Server()
{
	//deletes?
}

int Server::start(void)
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
		struct pollfd pfd = {fd, POLLIN, 0};
		_pollFds.push_back(pfd);
	}
	return (SUCCESS);
}

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

#include <poll.h>
/*
#include <vector>
int poll_loop()
{
	std::vector<struct pollfd> fds;
	
	// Ajouter le socket d'écoute
	struct pollfd listen_pfd;
	listen_pfd.fd	 = listen_fd;
	listen_pfd.events = POLLIN;   // On veut savoir quand une connexion arrive
	fds.push_back(listen_pfd);
	
	// Boucle principale
	while (true) {
		int ret = poll(fds.data(), fds.size(), 5000); // Timeout 5s
	
		if (ret == -1) {
			if (errno == EINTR) continue; // Signal, on relance
			perror("poll"); break;
		}
		if (ret == 0) continue; // Timeout, rien à faire
	
		for (size_t i = 0; i < fds.size(); ++i) {
			if (fds[i].revents == 0) continue; // Ce fd n'a rien
	
			if (fds[i].fd == listen_fd) {
				// Nouvelle connexion entrante
				handle_new_connection(listen_fd, fds);
			} else {
				// Activité sur un fd client
				handle_client(fds[i], fds, i);
			}
		}
	}
}

void add_to_poll(std::vector<struct pollfd>& fds, int fd, short events) {
	struct pollfd pfd;
	pfd.fd	  = fd;
	pfd.events  = events;
	pfd.revents = 0;
	fds.push_back(pfd);
}

// Retirer un fd (important: ne pas invalider les index en cours d'itération)
void remove_from_poll(std::vector<struct pollfd>& fds, int fd) {
	for (size_t i = 0; i < fds.size(); ++i) {
		if (fds[i].fd == fd) {
			fds.erase(fds.begin() + i);
			return;
		}
	}
}

*/
