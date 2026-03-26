#include "webserv.hpp"


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
