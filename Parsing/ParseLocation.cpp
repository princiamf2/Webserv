
#include "ParseConfig.hpp"
#include <iostream> //on en aura besoin pour afficher les erreurs et infos (cerr et cout)

static bool parseLocation_Root(std::istringstream& locationStream, Location& location) //parse le root (uri du dossier racine)
{
    std::string root_path;
    locationStream >> root_path;
    location.root = stripSemicolon(root_path);
    return true;
}

static bool parseLocation_Index(std::istringstream& locationStream, Location& location) //parse l'index (fichier index)
{
    std::string index_file;
    locationStream >> index_file;
    location.index = stripSemicolon(index_file);
    return true;
}

static bool parseLocation_Methods(std::istringstream& locationStream, Location& location) //parse les methodes GET, POST, DELETE
{
    std::string method;
    while (locationStream >> method)
    {
        method = stripSemicolon(method);
        location.allowed_methods_http.insert(method);
        if (method[method.size() - 1] == ';')
            break;
    }
    return true;
}

static bool parseLocation_ShowDirectory(std::istringstream& locationStream, Location& location) //(true ou false) afficher la liste des fichiers du dossier racine si pas d'index
{
    std::string show_directory_str;
    locationStream >> show_directory_str;
    show_directory_str = stripSemicolon(show_directory_str);
    if (show_directory_str == "true")
        location.show_directory = true;
    else if (show_directory_str == "false")
        location.show_directory = false;
    //verifier if ERROR
    return true;
}

static bool parseLocation_UploadDir(std::istringstream& locationStream, Location& location) // parse le dossier de upload des fichiers  (uri du dossier upload)
{
    std::string upload_dir;
    locationStream >> upload_dir;
    location.upload_dir = stripSemicolon(upload_dir);
    return true;
}

static bool parseLocation_RedirectPage(std::istringstream& locationStream, Location& location) //parse la page de redirection (code HTTP et URL)
{
    std::string code_str, url_str;
    locationStream >> code_str >> url_str;
    code_str = stripSemicolon(code_str);
    url_str  = stripSemicolon(url_str);
    std::istringstream in_string_stream(code_str);
    int code;
    in_string_stream >> code;
    //verifier if ERROR
    location.redirect_page = std::make_pair(code, url_str);
    return true;
}

static bool parseLocation_CgiExtensions(std::istringstream& locationStream, Location& location) //parser les extensions de fichiers pour lesquelles le CGI est actif (ex: .py ou .php)
{
    std::string cgi_extension;
    while (locationStream >> cgi_extension)
    {
        cgi_extension = stripSemicolon(cgi_extension);
        location.cgi_extensions.insert(cgi_extension);
        if (cgi_extension[cgi_extension.size() - 1] == ';')
            break;
    }
    return true;
}

bool parseLocation(std::istringstream& lineStream, std::istringstream& stream, ServerConfig& server) //parse une location et l'ajoute a la liste des locations du serveur
{
    Location location;
    lineStream >> location.path;

    std::string locationLine;
    while (std::getline(stream, locationLine)) //lire le contenu de la location ligne par ligne
    {
        if (locationLine.find("}") != std::string::npos)
            break;
        std::istringstream locationStream(locationLine); //créer un flux pour lire la ligne de la location
        std::string location_word;
        locationStream >> location_word;

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
    }
    server.locations.push_back(location); //ajouter la location a la liste des locations du serveur
    return true;
}

//A FAIRE : ajouter des verifications d'erreur pour chaque parseur de location
//(ex: verifier que le code de redirection est un nombre valide, que les methodes sont valides, etc...)

