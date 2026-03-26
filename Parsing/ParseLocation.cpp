
#include "ParseConfig.hpp"
#include <iostream> //on en aura besoin pour afficher les erreurs et infos (cerr et cout)

static bool parseLocation_Root(std::istringstream& locationStream, Location& location)
{
    std::string root_path;
    if (!(locationStream >> root_path))
    {
        std::cerr << "Error: missing location root" << std::endl;
        return false;
    }
    location.root = stripSemicolon(root_path);
    if (location.root.empty())
    {
        std::cerr << "Error: invalid location root" << std::endl;
        return false;
    }
    return true;
}

static bool parseLocation_Index(std::istringstream& locationStream, Location& location)
{
    std::string index_file;
    if (!(locationStream >> index_file))
    {
        std::cerr << "Error: missing location index" << std::endl;
        return false;
    }
    location.index = stripSemicolon(index_file);
    if (location.index.empty())
    {
        std::cerr << "Error: invalid location index" << std::endl;
        return false;
    }
    return true;
}

static bool parseLocation_Methods(std::istringstream& locationStream, Location& location) //parse les methodes GET, POST, DELETE
{
    std::string method;
    while (locationStream >> method)
    {
        method = stripSemicolon(method);
		if (method != "GET" && method != "POST" && method != "DELETE")
		{
			std::cerr << "Error: invalid method " << method << std::endl;
			return false;
		}
        location.allowed_methods_http.insert(method);
    }
    if (location.allowed_methods_http.empty())
    {
        std::cerr << "Error: no valid methods specified" << std::endl;
        return false;
    }
    return true;
}

static bool parseLocation_ShowDirectory(std::istringstream& locationStream, Location& location) //(true ou false) afficher la liste des fichiers du dossier racine si pas d'index
{
	std::string show_directory_str;
	if (!(locationStream >> show_directory_str))
	{
    	std::cerr << "Error: missing show_directory value" << std::endl;
    	return false;
	}
	show_directory_str = stripSemicolon(show_directory_str);
	if (show_directory_str == "true")
		location.show_directory = true;
	else if (show_directory_str == "false")
		location.show_directory = false;
	else
	{
		std::cerr << "Error: invalid value show_directory " << show_directory_str << std::endl;
		return false;
	}
	return true;
}

static bool parseLocation_UploadDir(std::istringstream& locationStream, Location& location) // parse le dossier de upload des fichiers  (uri du dossier upload)
{
	std::string upload_dir;
	if (!(locationStream >> upload_dir))
	{
    	std::cerr << "Error: missing upload_dir value" << std::endl;
    	return false;
	}
	location.upload_dir = stripSemicolon(upload_dir);
	if (location.upload_dir.empty())
	{
		std::cerr << "Error: invalid upload_dir" << std::endl;
		return false;
	}
	return true;
}

static bool parseLocation_RedirectPage(std::istringstream& locationStream, Location& location) //parse la page de redirection (code HTTP et URL)
{
	std::string code_str, url_str;
	if (!(locationStream >> code_str >> url_str))
	{
	    std::cerr << "Error: missing redirect_page values" << std::endl;
	    return false;
	}
	code_str = stripSemicolon(code_str);
	url_str  = stripSemicolon(url_str);
	std::istringstream in_string_stream(code_str);
	int code;

	if (!(in_string_stream >> code))
    {
        std::cerr << "Error: invalid redirect code: " << code_str << std::endl;
        return false;
    }

    in_string_stream >> std::ws;
    if (in_string_stream.peek() != EOF)
    {
        std::cerr << "Error: invalid redirect code: " << code_str << std::endl;
        return false;
    }

	if (code < 300 || code > 399)
	{
		std::cerr << "Error: redirect code must be between 300-399: " << code << std::endl;
		return false;
	}
	if (url_str.empty())
	{
   		std::cerr << "Error: invalid redirect url" << std::endl;
   		return false;
	}
	location.redirect_page = std::make_pair(code, url_str);
	return true;
}

static bool parseLocation_CgiExtensions(std::istringstream& locationStream, Location& location) //parser les extensions de fichiers pour lesquelles le CGI est actif (ex: .py ou .php)
{
    std::string cgi_extension;
    while (locationStream >> cgi_extension)
    {
        cgi_extension = stripSemicolon(cgi_extension);
		if (cgi_extension.empty())
		{
			std::cerr << "Error: invalid CGI extension" << std::endl;
			return false;
		}
        location.cgi_extensions.insert(cgi_extension);
    }
    return true;
}

bool parseLocation(std::istringstream& lineStream, std::istringstream& stream, ServerConfig& server) //parse une location et l'ajoute a la liste des locations du serveur
{
    Location location;

    if (!(lineStream >> location.path))
    {
        std::cerr << "Error: missing location path" << std::endl;
        return false;
    }
    if (location.path.empty() || location.path[0] != '/')
    {
        std::cerr << "Error: invalid location path" << std::endl;
        return false;
    }
    if (!expectOpenBracket(lineStream, stream))
    {
        return false;
    }
	bool closed = false;
	std::string locationLine;
	while (std::getline(stream, locationLine)) //lire le contenu de la location ligne par ligne
	{
		std::istringstream locationStream(locationLine);
		std::string location_word;
		locationStream >> location_word;
		if (location_word.empty() || location_word.substr(0, 2) == "//")
    		continue;
		if (location_word == "}")
		{
		    closed = true;
		    break;
		}
		if      (location_word == "root")
		{
			if (!parseLocation_Root(locationStream, location))
				return false;
		}
		else if (location_word == "index")
		{
			if (!parseLocation_Index(locationStream, location))
				return false;
		}
		else if (location_word == "methods")
		{
			if (!parseLocation_Methods(locationStream, location))
				return false;
		}
		else if (location_word == "show_directory")
		{
			if (!parseLocation_ShowDirectory(locationStream, location))
				return false;
		}
		else if (location_word == "upload_dir")
		{
			if (!parseLocation_UploadDir(locationStream, location))
				return false;
		}
		else if (location_word == "redirect_page")
		{
			if (!parseLocation_RedirectPage(locationStream, location))
				return false;
		}
		else if (location_word == "cgi_extensions")
		{
			if (!parseLocation_CgiExtensions(locationStream, location))
				return false;
		}
		else
		{
			std::cerr << "Error: unknown location directive: " << location_word << std::endl;
			return false;
		}
	}
	if (!closed)
	{
		std::cerr << "Error: not closed by }" << std::endl;
		return false;
	}
	server.locations.push_back(location); //ajouter la location a la liste des locations du serveur
	return true;
}


