
#include "ParseConfig.hpp"
#include <iostream> //on en aura besoin pour afficher les erreurs et infos (cerr et cout)

bool parseServer_Listen(std::istringstream& lineStream, ServerConfig& server)
{
    std::string port_str;
    lineStream >> port_str;
    port_str = port_str.substr(0, port_str.find(";"));

    std::istringstream in_string_stream(port_str);
    unsigned int port;

    if (!(in_string_stream >> port))
    {
        std::cerr << "Error: Invalid port number: " << port_str << std::endl;
        return false;
    }

    in_string_stream >> std::ws;
    if (in_string_stream.peek() != EOF)
    {
        std::cerr << "Error: Invalid port number: " << port_str << std::endl;
        return false;
    }

    if (port < 1 || port > 65535)
    {
        std::cerr << "Error: port must be between 1-65535: " << port << std::endl;
        return false;
    }

    server.listen_ports.insert(port);
    return true;
}

static bool parseServer_Root(std::istringstream& lineStream, ServerConfig& server) //parse le root (uri du dossier racine) du serveur
{
	std::string root_path;

	if (!(lineStream >> root_path))
	{
	    std::cerr << "Error: missing root " << std::endl;
	    return false;
	}
	server.root = stripSemicolon(root_path);
	if (server.root.empty())
	{
	    std::cerr << "Error: invalid root" << std::endl;
	    return false;
	}
	return true;
}

static bool parseServer_DomainName(std::istringstream& lineStream, ServerConfig& server)
{
    std::string domain_name;
    if (!(lineStream >> domain_name))
    {
        std::cerr << "Error: missing domain_name" << std::endl;
        return false;
    }

    domain_name = stripSemicolon(domain_name);
    if (domain_name.empty())
    {
        std::cerr << "Error: invalid domain_name" << std::endl;
        return false;
    }

    server.domain_names.insert(domain_name);
    return true;
}

static bool parseServer_Index(std::istringstream& lineStream, ServerConfig& server)
{
    std::string index_file;
    if (!(lineStream >> index_file))
    {
        std::cerr << "Error: missing index" << std::endl;
        return false;
    }

    server.index = stripSemicolon(index_file);
    if (server.index.empty())
    {
        std::cerr << "Error: invalid index" << std::endl;
        return false;
    }

    return true;
}

//parse les pages d'erreur du serveur (code d'erreur et chemin de la page d'erreur) et les ajoute a la map des pages d'erreur du serveur
static bool parseServer_ErrorPage(std::istringstream& lineStream, ServerConfig& server)
{
    std::string code_error_str, path_error;
    if (!(lineStream >> code_error_str >> path_error))
    {
        std::cerr << "Error: missing error_page values" << std::endl;
        return false;
    }
    code_error_str = stripSemicolon(code_error_str);
    path_error = stripSemicolon(path_error);
	if (path_error.empty())
	{
		std::cerr << "Error: invalid error_page path" << std::endl;
		return false;
	}

    std::istringstream in_string_stream(code_error_str);
    int code_error;

    if (!(in_string_stream >> code_error))
    {
        std::cerr << "Error: invalid error code: " << code_error_str << std::endl;
        return false;
    }

    in_string_stream >> std::ws;
    if (in_string_stream.peek() != EOF)
    {
        std::cerr << "Error: invalid error code: " << code_error_str << std::endl;
        return false;
    }

    if (code_error < 100 || code_error > 599)
    {
        std::cerr << "Error: error code must be between 100-599: " << code_error << std::endl;
        return false;
    }

    server.error_pages[code_error] = path_error;
    return true;
}

static bool parseServer_ClientMaxBodySize(std::istringstream& lineStream, ServerConfig& server) //parse la taille maximale du corps requête client et l'ajoute au serveur
{
	std::string size_str;
	if (!(lineStream >> size_str))
	{
		std::cerr << "Error: missing client_max_body_size" << std::endl;
		return false;
	}
	size_str = stripSemicolon(size_str);
    std::istringstream in_string_stream(size_str);
    unsigned int size;

    if (!(in_string_stream >> size))
    {
        std::cerr << "Error: invalid client_max_body_size: " << size_str << std::endl;
        return false;
    }

    in_string_stream >> std::ws;
    if (in_string_stream.peek() != EOF)
    {
        std::cerr << "Error: invalid client_max_body_size: " << size_str << std::endl;
        return false;
    }
	if (size <= 0)
	{
		std::cerr << "Error: client_max_body_size must be > 0: " << size << std::endl;
		return false;
	}
	server.client_max_body_size = size;
	return true;
}


bool parseServer(std::istringstream& stream, ServerConfig& server) //parse un serveur et l'ajoute a la liste des serveurs
{
	std::string serverLine;
	bool closed = false;
	while (std::getline(stream, serverLine))
	{
		std::istringstream lineStream(serverLine);
		std::string word_to_parse;
		lineStream >> word_to_parse;
		if (word_to_parse.empty() || word_to_parse.substr(0, 2) == "//")
    		continue;
		if (word_to_parse == "}")
		{
		    closed = true;
		    break;
		}
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
		else
		{
			std::cerr << "Error: unknown server directive: " << word_to_parse << std::endl;
			return false;
		}
	}
	if (!closed)
	{
		std::cerr << "Error: not closed by }" << std::endl;
		return false;
	}
	return true;
}
