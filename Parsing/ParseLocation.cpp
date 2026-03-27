
#include "Location.hpp"
#include "ParseConfig.hpp"
#include <iostream> //on en aura besoin pour afficher les erreurs et infos (cerr et cout)
#include <ostream>
#include <sstream>

static bool parseLocation_Root(std::istringstream& locationStream, Location& location)
{
    if (!location.root.empty())
    {
        std::cerr << "ERROR: DOUBLE location root" << std::endl;
        return false;
    }

    std::string root_path;
    if (!(locationStream >> root_path))
    {
        std::cerr << "ERROR: NO location root" << std::endl;
        return false;
    }

    if (!stripSemicolon(root_path, root_path))
    {
        std::cerr << "ERROR: WRONG location root syntax" << std::endl;
        return false;
    }

    if (root_path.empty())
    {
        std::cerr << "ERROR: WRONG location root" << std::endl;
        return false;
    }

    if (root_path[0] != '/')
    {
        std::cerr << "ERROR: WRONG location root must start by '/'" << std::endl;
        return false;
    }

    if (root_path.find("..") != std::string::npos)
    {
        std::cerr << "ERROR: WRONG location root cannot have '..'" << std::endl;
        return false;
    }

    std::string extra;
    if (locationStream >> extra)
    {
        std::cerr << "ERROR: CANNOT have token after location root: " << extra << std::endl;
        return false;
    }

    location.root = root_path;
    return true;
}

static bool parseLocation_Index(std::istringstream& locationStream, Location& location)
{
    if (!location.index.empty())
    {
        std::cerr << "ERROR: DOUBLE location index" << std::endl;
        return false;
    }

    std::string index_file;
    if (!(locationStream >> index_file))
    {
        std::cerr << "ERROR: NO location index" << std::endl;
        return false;
    }

    if (!stripSemicolon(index_file, index_file))
    {
        std::cerr << "ERROR: WRONG location index syntax" << std::endl;
        return false;
    }

    if (index_file.empty())
    {
        std::cerr << "ERROR: WRONG location index" << std::endl;
        return false;
    }

    std::string extra;
    if (locationStream >> extra)
    {
        std::cerr << "ERROR: unexpected token after location index: " << extra << std::endl;
        return false;
    }

    location.index = index_file;
    return true;
}

static bool parseLocation_Methods(std::istringstream& locationStream, Location& location)
{
    if (!location.allowed_methods_http.empty())
    {
        std::cerr << "ERROR: duplicate methods directive" << std::endl;
        return false;
    }

    std::string method;
    bool found = false;
    bool ended = false;

    while (locationStream >> method)
    {
        if (ended)
        {
            std::cerr << "ERROR: unexpected token after methods: " << method << std::endl;
            return false;
        }

        found = true;

        size_t pos = method.find(';');
        if (pos != std::string::npos)
        {
            if (!stripSemicolon(method, method))
            {
                std::cerr << "ERROR: WRONG methods syntax" << std::endl;
                return false;
            }
            ended = true;
        }

        if (method != "GET" && method != "POST" && method != "DELETE")
        {
            std::cerr << "ERROR: WRONG method " << method << std::endl;
            return false;
        }

        location.allowed_methods_http.insert(method);
    }

    if (!found)
    {
        std::cerr << "ERROR: NO valid methods" << std::endl;
        return false;
    }

    if (!ended)
    {
        std::cerr << "ERROR: NO ';' after methods" << std::endl;
        return false;
    }

    return true;
}

static bool parseLocation_ShowDirectory(std::istringstream& locationStream, Location& location)
{
    if (location.show_directory_set)
    {
        std::cerr << "ERROR: DOUBLE show_directory" << std::endl;
        return false;
    }

    std::string show_directory_str;
    if (!(locationStream >> show_directory_str))
    {
        std::cerr << "ERROR: NO show_directory value" << std::endl;
        return false;
    }

    if (!stripSemicolon(show_directory_str, show_directory_str))
    {
        std::cerr << "ERROR: WRONG show_directory syntax" << std::endl;
        return false;
    }

    if (show_directory_str == "true")
        location.show_directory = true;
    else if (show_directory_str == "false")
        location.show_directory = false;
    else
    {
        std::cerr << "ERROR: WRONG show_directory value " << show_directory_str << std::endl;
        return false;
    }

    std::string extra;
    if (locationStream >> extra)
    {
        std::cerr << "ERROR: CANNOT have wrong token after show_directory: " << extra << std::endl;
        return false;
    }

    location.show_directory_set = true;
    return true;
}

static bool parseLocation_UploadDir(std::istringstream& locationStream, Location& location)
{
    if (!location.upload_dir.empty())
    {
            std::cerr << "ERROR: DOUBLE upload_dir" << std::endl;
            return false;
    }

    std::string upload_dir;
    if (!(locationStream >> upload_dir))
    {
        std::cerr << "ERROR: NO upload_dir value" << std::endl;
        return false;
    }

    if (!stripSemicolon(upload_dir, upload_dir))
    {
        std::cerr << "ERROR: WRONG upload_dir syntax" << std::endl;
        return false;
    }

    if (upload_dir.empty())
    {
        std::cerr << "ERROR: WRONG upload_dir" << std::endl;
        return false;
    }

    if (upload_dir[0] != '/')
    {
        std::cerr << "ERROR: WRONG upload_dir must start with '/'" << std::endl;
        return false;
    }

    if (upload_dir.find("..") != std::string::npos)
    {
        std::cerr << "ERROR: WRONG upload_dir cannot contain '..'" << std::endl;
        return false;
    }

    std::string extra;
    if (locationStream >> extra)
    {
        std::cerr << "ERROR: CANNOT have token after upload_dir: " << extra << std::endl;
        return false;
    }

    location.upload_dir = upload_dir;
    return true;
}

static bool parseLocation_RedirectPage(std::istringstream& locationStream, Location& location)
{
    if (location.redirect_page.first != 0 || !location.redirect_page.second.empty())
    {
        std::cerr << "ERROR: DOUBLE redirect_page dir" << std::endl;
        return false;
    }

    std::string code_str, url_str;
    if (!(locationStream >> code_str >> url_str))
    {
        std::cerr << "ERROR: NO redirect_page values" << std::endl;
        return false;
    }

    if (!stripSemicolon(url_str, url_str))
    {
        std::cerr << "ERROR: WRONG redirect url syntax" << std::endl;
        return false;
    }

    if (url_str.empty())
    {
        std::cerr << "ERROR: WRONG redirect url" << std::endl;
        return false;
    }

    if (url_str[0] != '/' && url_str.find("http://") != 0 )
    {
        std::cerr << "ERROR: WRONG redirect url must start with '/' or 'http://'" << std::endl;
        return false;
    }

    if (url_str.find("..") != std::string::npos)
    {
        std::cerr << "ERROR: WRONG redirect url cannot contain '..'" << std::endl;
        return false;
    }

    std::istringstream in_string_stream(code_str);
    int code;

    if (!(in_string_stream >> code))
    {
        std::cerr << "ERROR: WRONG redirect code: " << code_str << std::endl;
        return false;
    }

    in_string_stream >> std::ws;
    if (in_string_stream.peek() != std::char_traits<char>::eof())
    {
        std::cerr << "ERROR: WRONG redirect code: " << code_str << std::endl;
        return false;
    }

    if (code < 300 || code > 399)
    {
        std::cerr << "ERROR: WRONG redirect code must be between 300-399: " << code << std::endl;
        return false;
    }

    std::string extra;
    if (locationStream >> extra)
    {
        std::cerr << "ERROR: CANNOT have token after redirect_page: " << extra << std::endl;
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
            std::cerr << "ERROR: CANNOT have token after cgi_extensions: " << cgi_extension << std::endl;
            return false;
        }

        found = true;

        size_t pos = cgi_extension.find(';');
        if (pos != std::string::npos)
        {
            if (!stripSemicolon(cgi_extension, cgi_extension))
            {
                std::cerr << "ERROR: WRONG CGI extension syntax" << std::endl;
                return false;
            }
            ended = true;
        }

        if (cgi_extension.empty())
        {
            std::cerr << "ERROR: WRONG CGI extension" << std::endl;
            return false;
        }

        if (cgi_extension[0] != '.')
        {
            std::cerr << "ERROR: CGI extension must start with '.': " << cgi_extension << std::endl;
            return false;
        }

        location.cgi_extensions.insert(cgi_extension);
    }

    if (!found)
    {
        std::cerr << "ERROR: NO CGI extensions" << std::endl;
        return false;
    }

    if (!ended)
    {
        std::cerr << "ERROR: NO ';' after cgi_extensions" << std::endl;
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
        std::cerr << "ERROR: NO CGI interpreter value" << std::endl;
        return false;
    }

    if (!stripSemicolon(interpreter, interpreter))
    {
        std::cerr << "ERROR: WRONG CGI interpreter syntax" << std::endl;
        return false;
    }

    if (extension.empty() || extension[0] != '.')
    {
        std::cerr << "ERROR: WRONG CGI extension" << std::endl;
        return false;
    }

    if (interpreter.empty())
    {
        std::cerr << "ERROR: WRONG CGI interpreter" << std::endl;
        return false;
    }

    if (location.cgi_interpreters.find(extension) != location.cgi_interpreters.end())
    {
        std::cerr << "ERROR: DOUBLE CGI interpreter for extension " << extension << std::endl;
        return false;
    }

    std::string extra;
    if (locationStream >> extra)
    {
        std::cerr << "ERROR: CANNOT have token after cgi_interpreter: " << extra << std::endl;
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
        std::cerr << "ERROR: NO location path" << std::endl;
        return false;
    }
    if (location.path.empty() || location.path[0] != '/')
    {
        std::cerr << "ERROR: WRONG location path" << std::endl;
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
			std::cerr << "ERROR: UNKNOWN location directive: " << location_word << std::endl;
			return false;
		}
	}
	if (!closed)
	{
		std::cerr << "ERROR: NOT closed by }" << std::endl;
		return false;
	}
	if (location.root.empty() && server.root.empty())
	{
		std::cerr << "ERROR: location has no root and server has no root" << std::endl;
		return false;
	}
	for (std::set<std::string>::const_iterator iter = location.cgi_extensions.begin();
		iter != location.cgi_extensions.end(); ++iter)
	{
		if (location.cgi_interpreters.find(*iter) == location.cgi_interpreters.end())
		{
			std::cerr << "ERROR: NO CGI interpreter for extension " << *iter << std::endl;
			return false;
		}
	}
	for (size_t i = 0; i < server.locations.size(); ++i)
	{
		if (server.locations[i].path == location.path)
		{
			std::cerr << "ERROR: DOUBLE location path " << location.path << std::endl;
			return false;
		}
	}
	server.locations.push_back(location); //ajouter la location a la liste des locations du serveur
	return true;
}
