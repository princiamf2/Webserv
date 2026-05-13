
#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include <string>
#include <set>
#include <map>
#include <vector>
#include "Location.hpp"

struct ListenEntry
{
	std::string interface;
	unsigned int port;

	bool operator<(ListenEntry const& other) const
	{
		if (interface != other.interface)
			return interface < other.interface;
		return port < other.port;
	}
};

struct ServerConfig
{
	std::set<unsigned int> listen_ports; //ports d'ecoute
	std::set<ListenEntry> listen_entries; //ports d'ecoute avec interface
	std::set<std::string> domain_names; //noms de domaine
	std::string root; //chemin dossier racine
	std::string index; //fichier index par defaut
	std::map<int, std::string> error_pages; //code d'erreur + chemin (page 404 par exemple)
	unsigned int client_max_body_size; //taille max du corps de la requete
	std::vector <Location> locations; //liste des locations pour ce serveur
	bool client_max_body_size_set; //indique si client_max_body_size a déjà été assigné

	ServerConfig() : client_max_body_size(0), client_max_body_size_set(false) {} //CPP 98
};

#endif 
