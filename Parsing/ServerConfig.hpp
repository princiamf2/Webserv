
#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include <string>
#include <set>
#include <map>
#include <vector>
#include "Location.hpp"

struct ServerConfig
{
	std::set<unsigned int> listen_ports; //ports d'ecoute
	std::set<std::string> domain_names; //noms de domaine
	std::string root; //chemin dossier racine
	std::string index; //fichier index par defaut
	std::map<int, std::string> error_pages; //code d'erreur + chemin (page 404 par exemple)
	unsigned int client_max_body_size; //taille max du corps de la requete
	std::vector <Location> locations; //liste des locations pour ce serveur

	ServerConfig() : client_max_body_size(0) {} //CPP 98
};

#endif 