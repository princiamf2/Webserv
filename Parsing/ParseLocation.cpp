
#include "Location.hpp"
#include "ParseConfig.hpp"
#include <iostream> //on en aura besoin pour afficher les erreurs et infos (cerr et cout)
#include <ostream>
#include <sstream>

static bool parseLocation_Root(std::istringstream& locationStream, Location& location)
{
    if (!location.root.empty())
    {
        std::cerr << "ERROR: DOUBLE LOCATION ROOT" << std::endl;
        return false;
    }

    std::string root_path;
    if (!(locationStream >> root_path))
    {
        std::cerr << "ERROR: NO LOCATION ROOT" << std::endl;
        return false;
    }

    if (!stripSemicolon(root_path, root_path))
    {
        std::cerr << "ERROR: WRONG LOCATION ROOT SYNTAX" << std::endl;
        return false;
    }

    if (root_path.empty())
    {
        std::cerr << "ERROR: WRONG LOCATION ROOT" << std::endl;
        return false;
    }

    // if (root_path[0] != '/')
    // {
    //     std::cerr << "ERROR: LOCATION ROOT MUST START BY '/'" << std::endl;
    //     return false;
    // }

    if (root_path.find("..") != std::string::npos)
    {
        std::cerr << "ERROR: LOCATION ROOT CANNOT CONTAIN '..'" << std::endl;
        return false;
    }

    std::string extra;
    if (locationStream >> extra)
    {
        std::cerr << "ERROR: CANNOT HAVE TOKEN AFTER LOCATION ROOT: " << extra << std::endl;
        return false;
    }

    location.root = root_path;
    return true;
}

static bool parseLocation_Index(std::istringstream& locationStream, Location& location)
{
    if (!location.index.empty())
    {
        std::cerr << "ERROR: DOUBLE LOCATION INDEX" << std::endl;
        return false;
    }

    std::string index_file;
    if (!(locationStream >> index_file))
    {
        std::cerr << "ERROR: NO LOCATION INDEX" << std::endl;
        return false;
    }

    if (!stripSemicolon(index_file, index_file))
    {
        std::cerr << "ERROR: WRONG LOCATION INDEX SYNTAX" << std::endl;
        return false;
    }

    if (index_file.empty())
    {
        std::cerr << "ERROR: WRONG LOCATION INDEX" << std::endl;
        return false;
    }

    std::string extra;
    if (locationStream >> extra)
    {
        std::cerr << "ERROR: CANNOT HAVE TOKEN AFTER LOCATION INDEX: " << extra << std::endl;
        return false;
    }

    location.index = index_file;
    return true;
}

static bool parseLocation_Methods(std::istringstream& locationStream, Location& location)
{
    if (!location.allowed_methods_http.empty())
    {
        std::cerr << "ERROR: DOUBLE LOCATION METHODS " << std::endl;
        return false;
    }

    std::string method;
    bool found = false;
    bool ended = false;

    while (locationStream >> method)
    {
        if (ended)
        {
            std::cerr << "ERROR: CANNOT HAVE TOKEN AFTER METHODS: " << method << std::endl;
            return false;
        }

        found = true;

        size_t pos = method.find(';');
        if (pos != std::string::npos)
        {
            if (!stripSemicolon(method, method))
            {
                std::cerr << "ERROR: WRONG METHODS SYNTAX" << std::endl;
                return false;
            }
            ended = true;
        }

        if (method != "GET" && method != "POST" && method != "DELETE")
        {
            std::cerr << "ERROR: WRONG METHOD " << method << std::endl;
            return false;
        }

        if (location.allowed_methods_http.find(method) != location.allowed_methods_http.end())
        {
            std::cerr << "ERROR: DOUBLE METHOD " << method << std::endl;
            return false;
        }

        location.allowed_methods_http.insert(method);
    }

    if (!found)
    {
        std::cerr << "ERROR: NO VALID METHODS" << std::endl;
        return false;
    }

    if (!ended)
    {
        std::cerr << "ERROR: NO ';' AFTER METHODS" << std::endl;
        return false;
    }

    return true;
}

static bool parseLocation_ShowDirectory(std::istringstream& locationStream, Location& location)
{
    if (location.show_directory_set)
    {
        std::cerr << "ERROR: DOUBLE SHOW_DIRECTORY" << std::endl;
        return false;
    }

    std::string show_directory_str;
    if (!(locationStream >> show_directory_str))
    {
        std::cerr << "ERROR: NO SHOW_DIRECTORY VALUE" << std::endl;
        return false;
    }

    if (!stripSemicolon(show_directory_str, show_directory_str))
    {
        std::cerr << "ERROR: WRONG SHOW_DIRECTORY SYNTAX" << std::endl;
        return false;
    }

    if (show_directory_str == "true")
        location.show_directory = true;
    else if (show_directory_str == "false")
        location.show_directory = false;
    else
    {
        std::cerr << "ERROR: WRONG SHOW_DIRECTORY VALUE " << show_directory_str << std::endl;
        return false;
    }

    std::string extra;
    if (locationStream >> extra)
    {
        std::cerr << "ERROR: CANNOT HAVE TOKEN AFTER SHOW_DIRECTORY: " << extra << std::endl;
        return false;
    }

    location.show_directory_set = true;
    return true;
}

static bool parseLocation_UploadDir(std::istringstream& locationStream, Location& location)
{
    if (!location.upload_dir.empty())
    {
            std::cerr << "ERROR: DOUBLE UPLOAD_DIR" << std::endl;
            return false;
    }

    std::string upload_dir;
    if (!(locationStream >> upload_dir))
    {
        std::cerr << "ERROR: NO UPLOAD_DIR VALUE" << std::endl;
        return false;
    }

    if (!stripSemicolon(upload_dir, upload_dir))
    {
        std::cerr << "ERROR: WRONG UPLOAD_DIR SYNTAX" << std::endl;
        return false;
    }

    if (upload_dir.empty())
    {
        std::cerr << "ERROR: WRONG UPLOAD_DIR" << std::endl;
        return false;
    }

    // if (upload_dir[0] != '/')
    // {
    //     std::cerr << "ERROR: WRONG UPLOAD_DIR MUST START BY '/'" << std::endl;
    //     return false;
    // }

    if (upload_dir.find("..") != std::string::npos)
    {
        std::cerr << "ERROR: WRONG UPLOAD_DIR CANNOT CONTAIN '..'" << std::endl;
        return false;
    }

    std::string extra;
    if (locationStream >> extra)
    {
        std::cerr << "ERROR: CANNOT HAVE TOKEN AFTER UPLOAD_DIR: " << extra << std::endl;
        return false;
    }

    location.upload_dir = upload_dir;
    return true;
}

static bool parseLocation_RedirectPage(std::istringstream& locationStream, Location& location)
{
    if (location.redirect_page.first != 0 || !location.redirect_page.second.empty())
    {
        std::cerr << "ERROR: DOUBLE REDIRECT_PAGE" << std::endl;
        return false;
    }

    std::string code_str, url_str;
    if (!(locationStream >> code_str >> url_str))
    {
        std::cerr << "ERROR: NO REDIRECT_PAGE VALUES" << std::endl;
        return false;
    }

    if (!stripSemicolon(url_str, url_str))
    {
        std::cerr << "ERROR: WRONG REDIRECT_PAGE URL SYNTAX" << std::endl;
        return false;
    }

    if (url_str.empty())
    {
        std::cerr << "ERROR: WRONG REDIRECT_PAGE URL" << std::endl;
        return false;
    }

    if (url_str[0] != '/' && url_str.find("http://") != 0 )
    {
        std::cerr << "ERROR: WRONG REDIRECT_PAGE URL MUST START WITH '/' OR 'http://'" << std::endl;
        return false;
    }

    if (url_str.find("..") != std::string::npos)
    {
        std::cerr << "ERROR: WRONG REDIRECT_PAGE URL CANNOT CONTAIN '..'" << std::endl;
        return false;
    }

    std::istringstream in_string_stream(code_str);
    int code;

    if (!(in_string_stream >> code))
    {
        std::cerr << "ERROR: WRONG REDIRECT_PAGE CODE: " << code_str << std::endl;
        return false;
    }

    in_string_stream >> std::ws;
    if (in_string_stream.peek() != std::char_traits<char>::eof())
    {
        std::cerr << "ERROR: WRONG REDIRECT_PAGE CODE: " << code_str << std::endl;
        return false;
    }

    if (code < 300 || code > 399)
    {
        std::cerr << "ERROR: WRONG REDIRECT_PAGE CODE MUST BE BETWEEN 300-399: " << code << std::endl;
        return false;
    }

    std::string extra;
    if (locationStream >> extra)
    {
        std::cerr << "ERROR: CANNOT HAVE TOKEN AFTER REDIRECT_PAGE: " << extra << std::endl;
        return false;
    }

    location.redirect_page = std::make_pair(code, url_str);
    return true;
}

static bool parseLocation_CgiExtensions(std::istringstream& locationStream, Location& location)
{
    std::string cgi_extension;
    bool found = false;
    bool ended = false;

    while (locationStream >> cgi_extension)
    {
        if (ended)
        {
            std::cerr << "ERROR: CANNOT HAVE TOKEN AFTER CGI_EXTENSIONS: " << cgi_extension << std::endl;
            return false;
        }

        found = true;

        size_t pos = cgi_extension.find(';');
        if (pos != std::string::npos)
        {
            if (!stripSemicolon(cgi_extension, cgi_extension))
            {
                std::cerr << "ERROR: WRONG CGI EXTENSION SYNTAX" << std::endl;
                return false;
            }
            ended = true;
        }

        if (cgi_extension.empty())
        {
            std::cerr << "ERROR: WRONG CGI EXTENSION" << std::endl;
            return false;
        }

        if (cgi_extension[0] != '.')
        {
            std::cerr << "ERROR: CGI EXTENSION MUST START BY '.': " << cgi_extension << std::endl;
            return false;
        }

        if (location.cgi_extensions.find(cgi_extension) != location.cgi_extensions.end())
        {
            std::cerr << "ERROR: DOUBLE CGI EXTENSION " << cgi_extension << std::endl;
            return false;
        }

        location.cgi_extensions.insert(cgi_extension);
    }

    if (!found)
    {
        std::cerr << "ERROR: NO CGI EXTENSIONS" << std::endl;
        return false;
    }

    if (!ended)
    {
        std::cerr << "ERROR: NO ';' AFTER CGI_EXTENSIONS" << std::endl;
        return false;
    }

    return true;
}

static bool parseLocation_CgiInterpreter(std::istringstream& locationStream, Location& location)
{
    std::string extension;
    std::string interpreter;

    if (!(locationStream >> extension >> interpreter))
    {
        std::cerr << "ERROR: NO CGI_INTERPRETER VALUE" << std::endl;
        return false;
    }

    if (!stripSemicolon(interpreter, interpreter))
    {
        std::cerr << "ERROR: WRONG CGI_INTERPRETER SYNTAX" << std::endl;
        return false;
    }

    if (extension.empty() || extension[0] != '.')
    {
        std::cerr << "ERROR: WRONG CGI_EXTENSION" << std::endl;
        return false;
    }

    if (interpreter.empty())
    {
        std::cerr << "ERROR: WRONG CGI_INTERPRETER" << std::endl;
        return false;
    }

    if (location.cgi_interpreters.find(extension) != location.cgi_interpreters.end())
    {
        std::cerr << "ERROR: DOUBLE CGI_INTERPRETER FOR EXTENSION " << extension << std::endl;
        return false;
    }

    std::string extra;
    if (locationStream >> extra)
    {
        std::cerr << "ERROR: CANNOT HAVE TOKEN AFTER CGI_INTERPRETER: " << extra << std::endl;
        return false;
    }

    location.cgi_extensions.insert(extension);
    location.cgi_interpreters[extension] = interpreter;
    return true;
}

bool parseLocation(std::istringstream& lineStream, std::istringstream& stream, ServerConfig& server) //parse une location et l'ajoute a la liste des locations du serveur
{
    Location location;

    if (!(lineStream >> location.path))
    {
        std::cerr << "ERROR: NO LOCATION PATH" << std::endl;
        return false;
    }
    if (location.path.empty() || location.path[0] != '/')
    {
        std::cerr << "ERROR: WRONG LOCATION PATH" << std::endl;
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
        else if (location_word == "cgi_interpreter")
        {
            if (!parseLocation_CgiInterpreter(locationStream, location))
                return false;
        }
		else
		{
			std::cerr << "ERROR: UNKNOWN LOCATION DIR: " << location_word << std::endl;
			return false;
		}
	}
	if (!closed)
	{
		std::cerr << "ERROR: NOT CLOSED BY }" << std::endl;
		return false;
	}
	if (location.root.empty() && server.root.empty())
	{
		std::cerr << "ERROR: LOCATION HAS NO ROOT AND SERVER HAS NO ROOT" << std::endl;
		return false;
	}
	for (std::set<std::string>::const_iterator iter = location.cgi_extensions.begin();
		iter != location.cgi_extensions.end(); ++iter)
	{
		if (location.cgi_interpreters.find(*iter) == location.cgi_interpreters.end())
		{
			std::cerr << "ERROR: NO CGI_INTERPRETER FOR EXTENSION " << *iter << std::endl;
			return false;
		}
	}
	for (size_t i = 0; i < server.locations.size(); ++i)
	{
		if (server.locations[i].path == location.path)
		{
			std::cerr << "ERROR: DOUBLE LOCATION PATH " << location.path << std::endl;
			return false;
		}
	}
	server.locations.push_back(location); //ajouter la location a la liste des locations du serveur
	return true;
}
