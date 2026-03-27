
#include "ParseConfig.hpp"
#include <iostream> //on en aura besoin pour afficher les erreurs et infos (cerr et cout)

bool parseServer_Listen(std::istringstream& lineStream, ServerConfig& server)
{
	std::string port_str;
	lineStream >> port_str;

	if (port_str.empty())
	{
		std::cerr << "ERROR: NO listen port" << std::endl;
		return false;
	}
	if (!stripSemicolon(port_str, port_str))
	{
		std::cerr << "ERROR: WRONG listen syntax" << std::endl;
		return false;
	}

	std::istringstream in_string_stream(port_str);
	unsigned int port;

	if (!(in_string_stream >> port))
	{
		std::cerr << "ERROR: WRONG port number: " << port_str << std::endl;
		return false;
	}

	in_string_stream >> std::ws;
	if (in_string_stream.peek() != std::char_traits<char>::eof())
	{
		std::cerr << "ERROR: WRONG port number: " << port_str << std::endl;
		return false;
	}

	if (port < 1 || port > 65535)
	{
		std::cerr << "ERROR: port must be between 1-65535: " << port << std::endl;
		return false;
	}

	std::string extra;
	if (lineStream >> extra)
	{
		std::cerr << "ERROR: CANNOT HAVE TOKEN AFTER LISTEN: " << extra << std::endl;
		return false;
	}

	server.listen_ports.insert(port);
	return true;
}

static bool parseServer_Root(std::istringstream& lineStream, ServerConfig& server)
{
    if (!server.root.empty())
    {
        std::cerr << "ERROR: DOUBLE root" << std::endl;
        return false;
    }

    std::string value;
    if (!(lineStream >> value))
    {
        std::cerr << "ERROR: NO root" << std::endl;
        return false;
    }

    if (!stripSemicolon(value, value))
    {
        std::cerr << "ERROR: WRONG root syntax" << std::endl;
        return false;
    }

    if (value.empty())
    {
        std::cerr << "ERROR: WRONG root" << std::endl;
        return false;
    }

    if (value[0] != '/')
    {
        std::cerr << "ERROR:root must start by '/'" << std::endl;
        return false;
    }

    if (value.find("..") != std::string::npos)
    {
        std::cerr << "ERROR: root cannot contain '..'" << std::endl;
        return false;
    }

    std::string extra;
    if (lineStream >> extra)
    {
        std::cerr << "ERROR: CANNOT HAVE TOKEN AFTER ROOT: " << extra << std::endl;
        return false;
    }

    server.root = value;
    return true;
}

static bool parseServer_DomainName(std::istringstream& lineStream, ServerConfig& server)
{
    std::string domain_name;

    if (!(lineStream >> domain_name))
    {
        std::cerr << "ERROR: NO domain_name" << std::endl;
        return false;
    }

    if (!stripSemicolon(domain_name, domain_name))
    {
        std::cerr << "ERROR: WRONG domain_name syntax" << std::endl;
        return false;
    }

    if (domain_name.empty())
    {
        std::cerr << "ERROR: WRONG domain_name" << std::endl;
        return false;
    }

    std::string extra;
    if (lineStream >> extra)
    {
        std::cerr << "ERROR: CANNOT HAVE TOKEN AFTER DOMAIN_NAME: " << extra << std::endl;
        return false;
    }

    server.domain_names.insert(domain_name);
    return true;
}

static bool parseServer_Index(std::istringstream& lineStream, ServerConfig& server)
{
    if (!server.index.empty())
    {
        std::cerr << "ERROR: DOUBLE index" << std::endl;
        return false;
    }

    std::string index_file;
    if (!(lineStream >> index_file))
    {
        std::cerr << "ERROR: NO index" << std::endl;
        return false;
    }

    if (!stripSemicolon(index_file, index_file))
    {
        std::cerr << "ERROR: WRONG index syntax" << std::endl;
        return false;
    }

    if (index_file.empty())
    {
        std::cerr << "ERROR: WRONG index" << std::endl;
        return false;
    }

    std::string extra;
    if (lineStream >> extra)
    {
        std::cerr << "ERROR: CANNOT HAVE TOKEN AFTER INDEX: " << extra << std::endl;
        return false;
    }

    server.index = index_file;
    return true;
}

//parse les pages d'erreur du serveur (code d'erreur et chemin de la page d'erreur) et les ajoute a la map des pages d'erreur du serveur

static bool parseServer_ErrorPage(std::istringstream& lineStream, ServerConfig& server)
{
    std::string code_error_str, path_error;

    if (!(lineStream >> code_error_str >> path_error))
    {
        std::cerr << "ERROR: NO error_page values" << std::endl;
        return false;
    }

    if (!stripSemicolon(path_error, path_error))
    {
        std::cerr << "ERROR: WRONG error_page path syntax" << std::endl;
        return false;
    }

    if (path_error.empty())
    {
        std::cerr << "ERROR: WRONG error_page path" << std::endl;
        return false;
    }

    if (path_error[0] != '/')
    {
        std::cerr << "ERROR: WRONG error_page path must start with '/'" << std::endl;
        return false;
    }

    if (path_error.find("..") != std::string::npos)
    {
        std::cerr << "ERROR: WRONG error_page path cannot contain '..'" << std::endl;
        return false;
    }

    std::istringstream in_string_stream(code_error_str);
    int code_error;

    if (!(in_string_stream >> code_error))
    {
        std::cerr << "ERROR: WRONG error code: " << code_error_str << std::endl;
        return false;
    }

    in_string_stream >> std::ws;
    if (in_string_stream.peek() != std::char_traits<char>::eof())
    {
        std::cerr << "ERROR: WRONG error code: " << code_error_str << std::endl;
        return false;
    }

    if (code_error < 100 || code_error > 599)
    {
        std::cerr << "ERROR: WRONG error code must be between 100-599: " << code_error << std::endl;
        return false;
    }

    std::string extra;
    if (lineStream >> extra)
    {
        std::cerr << "ERROR: CANNOT HAVE TOKEN AFTER error_page: " << extra << std::endl;
        return false;
    }

    server.error_pages[code_error] = path_error;
    return true;
}

static bool parseServer_ClientMaxBodySize(std::istringstream& lineStream, ServerConfig& server)
{
    if (server.client_max_body_size_set)
    {
        std::cerr << "ERROR: DOUBLE client_max_body_size directive" << std::endl;
        return false;
    }

    std::string size_str;
    if (!(lineStream >> size_str))
    {
        std::cerr << "ERROR: NO client_max_body_size" << std::endl;
        return false;
    }

    if (!stripSemicolon(size_str, size_str))
    {
        std::cerr << "ERROR: WRONG client_max_body_size syntax" << std::endl;
        return false;
    }

    std::istringstream in_string_stream(size_str);
    unsigned int size;

    if (!(in_string_stream >> size))
    {
        std::cerr << "ERROR: WRONG client_max_body_size: " << size_str << std::endl;
        return false;
    }

    in_string_stream >> std::ws;
    if (in_string_stream.peek() != std::char_traits<char>::eof())
    {
        std::cerr << "ERROR: WRONG client_max_body_size: " << size_str << std::endl;
        return false;
    }

    if (size <= 0)
    {
        std::cerr << "ERROR: client_max_body_size must be > 0: " << size << std::endl;
        return false;
    }

    std::string extra;
    if (lineStream >> extra)
    {
        std::cerr << "ERROR: CANNOT HAVE TOKEN AFTER client_max_body_size: " << extra << std::endl;
        return false;
    }

    server.client_max_body_size_set = true;
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
			std::cerr << "ERROR: UNKNOWN server directive: " << word_to_parse << std::endl;
			return false;
		}
	}
	if (!closed)
	{
		std::cerr << "ERROR: NOT closed by }" << std::endl;
		return false;
	}
	if (server.listen_ports.empty())
	{
    	std::cerr << "ERROR: server has no listen directive" << std::endl;
    	return false;
	}
	if (server.root.empty())
	{
		std::cerr << "ERROR: server has no root directive" << std::endl;
		return false;
	}
	return true;
}
