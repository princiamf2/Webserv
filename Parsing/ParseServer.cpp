
#include "ParseConfig.hpp"
#include <iostream> //on en aura besoin pour afficher les erreurs et infos (cerr et cout)

bool parseServer_Listen(std::istringstream& lineStream, ServerConfig& server) //parse le port d'ecoute du serveur et l'ajoute a la liste des ports d'ecoute du serveur
{
    std::string port_str;
    lineStream >> port_str;
    port_str = port_str.substr(0, port_str.find(";"));
    std::istringstream in_string_stream(port_str);
    unsigned int port;
    in_string_stream >> port;
	//verifier if ERROR 
    server.listen_ports.insert(port);
    return true;
}

static bool parseServer_Root(std::istringstream& lineStream, ServerConfig& server) //parse le root (uri du dossier racine) du serveur
{
    std::string root_path;
    lineStream >> root_path;
    server.root = stripSemicolon(root_path);
    return true;
}

static bool parseServer_DomainName(std::istringstream& lineStream, ServerConfig& server) //parse les noms de domaine du serveur et les ajoute a la liste des noms de domaine du serveur
{
    std::string domain_name;
    lineStream >> domain_name;
    server.domain_names.insert(stripSemicolon(domain_name));
    return true;
}

static bool parseServer_Index(std::istringstream& lineStream, ServerConfig& server) //parse l'index (fichier index) du serveur
{
    std::string index_file;
    lineStream >> index_file;
    server.index = stripSemicolon(index_file);
    return true;
}

//parse les pages d'erreur du serveur (code d'erreur et chemin de la page d'erreur) et les ajoute a la map des pages d'erreur du serveur
static bool parseServer_ErrorPage(std::istringstream& lineStream, ServerConfig& server) 
{
    std::string code_error_str, path_error;
    lineStream >> code_error_str >> path_error;
    code_error_str = stripSemicolon(code_error_str);
    path_error = stripSemicolon(path_error);
    std::istringstream in_string_stream(code_error_str);
    int code_error;
    in_string_stream >> code_error;
	//verifier if ERROR
    server.error_pages[code_error] = path_error;
    return true;
}

static bool parseServer_ClientMaxBodySize(std::istringstream& lineStream, ServerConfig& server) //parse la taille maximale du corps requête client et l'ajoute au serveur
{
    std::string size_str;
    lineStream >> size_str;
    size_str = stripSemicolon(size_str);
    std::istringstream in_string_stream(size_str);
    unsigned int size;
    in_string_stream >> size;
	//verifier if ERROR
    server.client_max_body_size = size;
    return true;
}


bool parseServer(std::istringstream& stream, ServerConfig& server) //parse un serveur et l'ajoute a la liste des serveurs
{
    std::string serverLine;
    while (std::getline(stream, serverLine))
    {
        if (serverLine.find("}") != std::string::npos) //fin du serveur
            break;
        std::istringstream lineStream(serverLine);
        std::string word_to_parse;
        lineStream >> word_to_parse;

        if      (word_to_parse == "listen")               
        { 
            if (!parseServer_Listen(lineStream, server))
                return false; 
        }
        else if (word_to_parse == "root")
		{
			if (!parseServer_Root(lineStream, server))
				return false; 
		}
        else if (word_to_parse == "domain_name")
		{
			if (!parseServer_DomainName(lineStream, server))
				return false; 
		}
        else if (word_to_parse == "index")
		{
			if (!parseServer_Index(lineStream, server))
				return false; 
		}
        else if (word_to_parse == "error_page")
		{
			if (!parseServer_ErrorPage(lineStream, server))
				return false; 
		}
        else if (word_to_parse == "client_max_body_size") //pour parser la taille maximalede la requête client et l'ajouter au serveur
		{
			if (!parseServer_ClientMaxBodySize(lineStream, server))
				return false; 
		}
        else if (word_to_parse == "location")
		{
			if (!parseLocation(lineStream, stream, server))
				return false; 
		}
    }
    return true;
}

//A FAIRE : ajouter des verifications d'erreur pour chaque parseur 
//(ex: verifier que le port est un nombre valide, que la taille maximale du corps de la requete est un nombre valide, etc...)