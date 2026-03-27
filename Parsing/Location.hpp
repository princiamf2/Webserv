
#ifndef LOCATION_HPP
#define LOCATION_HPP

#include <string>
#include <set>
#include <utility> // pour std::pair
#include <map>

struct Location
{
	std::string path;
	std::set<std::string> allowed_methods_http; //pour GET, POST, DELETE
	std::string root; //chemin dossier racine
	std::string index; //fichier index par defaut
	bool show_directory; //affiche la liste des fichiers , si pas index 
	std::string upload_dir; //dossier pour les fichiers uploades
	std::pair <int, std::string> redirect_page; //code redirection + url redirection
	std::set <std::string> cgi_extensions; //lquel CGI est active .py ou .php par exemple
	std::map<std::string, std::string> cgi_interpreters;

	Location() : show_directory(false), redirect_page(std::make_pair(0, "")) {} //CPP 98
};

#endif 