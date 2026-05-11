
#include "ParseConfig.hpp"
#include <iostream> //on en aura besoin pour afficher les erreurs et infos (cerr et cout)

bool parseServer_Listen(std::istringstream& lineStream, ServerConfig& server)
{
	std::string port_str;
	lineStream >> port_str;

	if (port_str.empty())
	{
		std::cerr << "ERROR: NO LISTEN PORT" << std::endl;
		return false;
	}
	if (!stripSemicolon(port_str, port_str))
	{
		std::cerr << "ERROR: WRONG LISTEN SYNTAX" << std::endl;
		return false;
	}

	std::istringstream in_string_stream(port_str);
	unsigned int port;

	if (!(in_string_stream >> port))
	{
		std::cerr << "ERROR: WRONG PORT NUMBER: " << port_str << std::endl;
		return false;
	}

	in_string_stream >> std::ws;
	if (in_string_stream.peek() != std::char_traits<char>::eof())
	{
		std::cerr << "ERROR: WRONG PORT NUMBER: " << port_str << std::endl;
		return false;
	}

	if (port < 1 || port > 65535)
	{
		std::cerr << "ERROR: PORT MUST BE BETWEEN 1-65535: " << port << std::endl;
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
        std::cerr << "ERROR: DOUBLE ROOT" << std::endl;
        return false;
    }

    std::string value;
    if (!(lineStream >> value))
    {
        std::cerr << "ERROR: NO ROOT" << std::endl;
        return false;
    }

    if (!stripSemicolon(value, value))
    {
        std::cerr << "ERROR: WRONG ROOT SYNTAX" << std::endl;
        return false;
    }

    if (value.empty())
    {
        std::cerr << "ERROR: WRONG ROOT" << std::endl;
        return false;
    }

    // if (value[0] != '/')
    // {
    //     std::cerr << "ERROR: ROOT MUST START BY '/'" << std::endl;
    //     return false;
    // }

    if (value.find("..") != std::string::npos)
    {
        std::cerr << "ERROR: ROOT CANNOT CONTAIN '..'" << std::endl;
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
        std::cerr << "ERROR: NO DOMAIN_NAME" << std::endl;
        return false;
    }

    if (!stripSemicolon(domain_name, domain_name))
    {
        std::cerr << "ERROR: WRONG DOMAIN_NAME SYNTAX" << std::endl;
        return false;
    }

    if (domain_name.empty())
    {
        std::cerr << "ERROR: WRONG DOMAIN_NAME" << std::endl;
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
        std::cerr << "ERROR: DOUBLE INDEX" << std::endl;
        return false;
    }

    std::string index_file;
    if (!(lineStream >> index_file))
    {
        std::cerr << "ERROR: NO INDEX" << std::endl;
        return false;
    }

    if (!stripSemicolon(index_file, index_file))
    {
        std::cerr << "ERROR: WRONG INDEX SYNTAX" << std::endl;
        return false;
    }

    if (index_file.empty())
    {
        std::cerr << "ERROR: WRONG INDEX" << std::endl;
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
        std::cerr << "ERROR: NO ERROR_PAGE VALUES" << std::endl;
        return false;
    }

    if (!stripSemicolon(path_error, path_error))
    {
        std::cerr << "ERROR: WRONG ERROR_PAGE PATH SYNTAX" << std::endl;
        return false;
    }

    if (path_error.empty())
    {
        std::cerr << "ERROR: WRONG ERROR_PAGE PATH" << std::endl;
        return false;
    }

    if (path_error[0] != '/')
    {
        std::cerr << "ERROR: WRONG ERROR_PAGE PATH MUST START BY '/'" << std::endl;
        return false;
    }

    if (path_error.find("..") != std::string::npos)
    {
        std::cerr << "ERROR: WRONG ERROR_PAGE PATH CANNOT CONTAIN '..'" << std::endl;
        return false;
    }

    std::istringstream in_string_stream(code_error_str);
    int code_error;

    if (!(in_string_stream >> code_error))
    {
        std::cerr << "ERROR: WRONG ERROR CODE: " << code_error_str << std::endl;
        return false;
    }

    in_string_stream >> std::ws;
    if (in_string_stream.peek() != std::char_traits<char>::eof())
    {
        std::cerr << "ERROR: WRONG ERROR CODE: " << code_error_str << std::endl;
        return false;
    }

    if (code_error < 100 || code_error > 599)
    {
        std::cerr << "ERROR: WRONG ERROR CODE MUST BE BETWEEN 100-599: " << code_error << std::endl;
        return false;
    }

    std::string extra;
    if (lineStream >> extra)
    {
        std::cerr << "ERROR: CANNOT HAVE TOKEN AFTER ERROR_PAGE: " << extra << std::endl;
        return false;
    }

    server.error_pages[code_error] = path_error;
    return true;
}

static bool parseServer_ClientMaxBodySize(std::istringstream& lineStream, ServerConfig& server)
{
    if (server.client_max_body_size_set)
    {
        std::cerr << "ERROR: DOUBLE CLIENT_MAX_BODY_SIZE" << std::endl;
        return false;
    }

    std::string size_str;
    if (!(lineStream >> size_str))
    {
        std::cerr << "ERROR: NO CLIENT_MAX_BODY_SIZE" << std::endl;
        return false;
    }

    if (!stripSemicolon(size_str, size_str))
    {
        std::cerr << "ERROR: WRONG CLIENT_MAX_BODY_SIZE SYNTAX" << std::endl;
        return false;
    }

    std::istringstream in_string_stream(size_str);
    long long size;

    if (!(in_string_stream >> size))
    {
        std::cerr << "ERROR: WRONG CLIENT_MAX_BODY_SIZE: " << size_str << std::endl;
        return false;
    }

    in_string_stream >> std::ws;
    if (in_string_stream.peek() != std::char_traits<char>::eof())
    {
        std::cerr << "ERROR: WRONG CLIENT_MAX_BODY_SIZE: " << size_str << std::endl;
        return false;
    }

    if (size <= 0)
    {
        std::cerr << "ERROR: CLIENT_MAX_BODY_SIZE MUST BE > 0: " << size << std::endl;
        return false;
    }

    if (size > 2147483647)
    {
        std::cerr << "ERROR: CLIENT_MAX_BODY_SIZE TOO LARGE: " << size << std::endl;
        return false;
    }

    std::string extra;
    if (lineStream >> extra)
    {
        std::cerr << "ERROR: CANNOT HAVE TOKEN AFTER CLIENT_MAX_BODY_SIZE: " << extra << std::endl;
        return false;
    }

    server.client_max_body_size_set = true;
    server.client_max_body_size = static_cast<unsigned int>(size);
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
			std::cerr << "ERROR: UNKNOWN SERVER DIRECTIVE: " << word_to_parse << std::endl;
			return false;
		}
	}
	if (!closed)
	{
		std::cerr << "ERROR: NOT CLOSED BY }" << std::endl;
		return false;
	}
	if (server.listen_ports.empty())
	{
    	std::cerr << "ERROR: SERVER HAS NO LISTEN DIRECTIVE" << std::endl;
    	return false;
	}
	if (server.root.empty())
	{
		std::cerr << "ERROR: SERVER HAS NO ROOT DIRECTIVE" << std::endl;
		return false;
	}
	return true;
}
