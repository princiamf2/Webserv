
#include "ParseConfig.hpp"
#include <fstream> //permet de lire le fichier de config
#include <iostream>
#include <sstream> //permet de lire le contenu du configfile ligne par ligne


//funct qui parse configfile et retourne un vector de ServerConfig
std::vector<ServerConfig> parseConfig(std::string path)
{
	std::ifstream configFile(path);
	if(!configFile.is_open())
	{
		std::cerr << "Error opening config file " << path << std::endl;
		return std::vector<ServerConfig>();
	}
	
	std::string content((std::istreambuf_iterator<char>(configFile)), std::istreambuf_iterator<char>()); // lit tout le contenu du configfile dans une string
	//ifstreambuf_iterator parcourt un fichier char par char, premier pour le debut du fichier, second pour la fin du fichier
	if (content.empty())
	{
	    std::cerr << "config file is empty" << std::endl;
    	return std::vector<ServerConfig>();
	}

	// A FAIRE: analyser le contenu et remplir les ServerConfig
	std::istringstream stream(content); //charge le content 
	std::string line;
	std::vector<ServerConfig> serverlist; //vector de ServerConfig a retourner
	
	while (std::getline(stream, line)) //boucle principale pour lire le configfile ligne par ligne
	{
		if (line.find("server") != std::string::npos)
		{
			ServerConfig server;	
			std::string serverLine;
			while (std::getline(stream, serverLine)) 
			{
				if (serverLine.find("}") != std::string::npos)
					break;
				std::istringstream lineStream(serverLine);
				std::string word_to_parse;
				lineStream >> word_to_parse;
				if (word_to_parse == "listen")
				{
					std::string port_str;
					lineStream >> port_str; // extaire le port par exemple "0808;"
					port_str = port_str.substr(0, port_str.find(";"));
					std::istringstream in_string_stream(port_str);
					unsigned int port;
					in_string_stream >> port;
					if (in_string_stream.fail())
					{
						std::cerr << "invalid port: " << port_str << std::endl;
						return std::vector<ServerConfig>();
					}
					server.listen_ports.insert(port); 
				}
				else if (word_to_parse == "root")
				{
					std::string root_path;
					lineStream >> root_path; //recup path de root
					root_path = root_path.substr(0, root_path.find(";")); // enleve le ;
					server.root = root_path; // assigne a server.root
				}
				else if (word_to_parse == "domain_name")
				{
					std::string domain_name;
					lineStream >> domain_name; //recup domain_name
					domain_name = domain_name.substr(0, domain_name.find(";"));
					server.domain_names.insert(domain_name); //std::set donc insert pour ajouter a server.domain_names
				}
				
				else if (word_to_parse == "index")
				{
					std::string index_file;
					lineStream >> index_file;
					index_file = index_file.substr(0, index_file.find(";"));
					server.index = index_file; 
				}

				else if (word_to_parse == "error_page")
				{
					std::string code_error_str, path_error;
					lineStream >> code_error_str >> path_error; //extrait le code d'erreur et le chemin de la page d'erreur
					code_error_str = code_error_str.substr(0, code_error_str.find(";"));
					path_error = path_error.substr(0, path_error.find(";"));
					std::istringstream in_string_stream(code_error_str);
					int code_error;
					in_string_stream >> code_error;
					if (in_string_stream.fail())
					{
						std::cerr << "invalid error code: " << code_error_str << std::endl;
						return std::vector<ServerConfig>();
					}
					server.error_pages[code_error] = path_error; //ajoute le code d'erreur et le chemin de la page d'erreur a server.error_pages
				}
				else if (word_to_parse == "client_max_body_size")
				{
					std::string size_str;
					lineStream >> size_str; 
					size_str = size_str.substr(0, size_str.find(";"));
					std::istringstream in_string_stream(size_str);
					unsigned int size;
					in_string_stream >> size;
					if (in_string_stream.fail())
					{
						std::cerr << "invalid client_max_body_size: " << size_str << std::endl;
						return std::vector<ServerConfig>();
					}
					server.client_max_body_size = size;
				}
				
				else if (word_to_parse == "location") 
				{
					Location location;
					std::string locationLine;
					lineStream >> location.path;
					while (std::getline(stream, locationLine)) //seconde boucle pour lire les lignes location
					{
						if (locationLine.find("}") != std::string::npos)
							break;
						std::istringstream locationStream(locationLine);
						std::string location_word;
						locationStream >> location_word;

						if (location_word == "root")
						{
							std::string root_path;
							locationStream >> root_path; //recup path de root
							root_path = root_path.substr(0, root_path.find(";")); // enleve le ;
							location.root = root_path; // assigne a location.root
						}
						
						else if (location_word == "index")
						{
							std::string index_file;
							locationStream >> index_file;
							index_file = index_file.substr(0, index_file.find(";"));
							location.index = index_file; 
						}

						else if (location_word == "methods")
						{
    						std::string method;
    						while (locationStream >> method)
    						{
    			    			method = method.substr(0, method.find(";"));
    			    			location.allowed_methods_http.insert(method); // insert directement dans location
    			    			if (method[method.size() - 1] == ';') // C++98 compatible
    			        			break;
    						}
						}

						else if (location_word == "show_directory")
						{
							std::string show_directory_str;
							locationStream >> show_directory_str;
							show_directory_str = show_directory_str.substr(0, show_directory_str.find(";"));
							if (show_directory_str == "true")
								location.show_directory = true;
							else if (show_directory_str == "false")
								location.show_directory = false;
							else
							{
								std::cerr << "invalid value for show_directory: " << show_directory_str << std::endl;
								return std::vector<ServerConfig>();
							}
						}

						else if (location_word == "upload_dir")
						{
							std::string upload_dir;
							locationStream >> upload_dir;
							upload_dir = upload_dir.substr(0, upload_dir.find(";"));
							location.upload_dir = upload_dir;
						}

						else if (location_word == "redirect_page") 
						{
							std::string code_str, url_str;
							locationStream >> code_str >> url_str;
							code_str = code_str.substr(0, code_str.find(";"));
							url_str = url_str.substr(0, url_str.find(";"));
							std::istringstream in_string_stream(code_str);
							int code;
							in_string_stream >> code;
							if (in_string_stream.fail())
							{
								std::cerr << "invalid redirect code: " << code_str << std::endl;
								return std::vector<ServerConfig>();
							}
							location.redirect_page = std::make_pair(code, url_str);
						}

						else if (location_word == "cgi_extensions")
						{
							std::string cgi_extension;
							while (locationStream >> cgi_extension)
							{
								cgi_extension = cgi_extension.substr(0, cgi_extension.find(";"));
								location.cgi_extensions.insert(cgi_extension);
								if (cgi_extension[cgi_extension.size() - 1] == ';') // C++98 compatible
									break;
							}
						}
						
					}
					server.locations.push_back(location);
				}
				
			}
			serverlist.push_back(server);
		}
	}
	return serverlist;
}




/*

c++ -Wall -Wextra -Werror main.cpp ParseConfig.cpp -o webserv

*/