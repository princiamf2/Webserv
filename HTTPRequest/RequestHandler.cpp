#include "RequestHandler.hpp"
#include <cstddef>
#include <sstream>
#include <fstream>
#include <string>
#include <cstdio>
#include <sys/stat.h>
#include <dirent.h>
#include <vector>

//petit check de la taile du body
static bool isBodySizeValid(HttpRequest const& request, ServerConfig const& server)
{
	if (server.client_max_body_size == 0)
		return true;
	return (request.body.size() <= server.client_max_body_size);
}
//renvoi le bon root
static std::string resolveRoot(ServerConfig const& server, Location const* location)
{
	if (location && !location->root.empty())
		return location->root;
	return server.root;
}
//petit outil qui check les methods si la method on la supporte
static bool isSupportedMethod(std::string const& method)
{
    return (method == "GET" || method == "POST" || method == "DELETE");
}
//petit outil qui check si la methode et autoriser dans une location
static bool isMethodAllowed(std::string const& method, Location const* location)
{
	if (!location)
		return true;
	if (location->allowed_methods_http.empty())
		return true;
	return (location->allowed_methods_http.find(method) != location->allowed_methods_http.end());
}
//normalisation du chemin relatif
static bool normalizeRelativePath(std::string const& rawPath, std::string& normalized)
{
	std::vector<std::string> segments;
	size_t i = 0;

	while (i < rawPath.size())
	{
		while (i < rawPath.size() && rawPath[i] == '/')
			++i;
		size_t start = i;
		while (i < rawPath.size() && rawPath[i] != '/')
			++i;
		std::string part = rawPath.substr(start, i - start);
		if (part.empty() || part == ".")
			continue;
		if (part == "..")
		{
			if (segments.empty())
				return false;
			segments.pop_back();
			continue;
		}
		segments.push_back(part);
	}
	normalized.clear();
	for (size_t j = 0; j < segments.size(); ++j)
		normalized += "/" + segments[j];
	if (normalized.empty())
		normalized = "/";
	return true;
}
static bool locationMatches(std::string const& requestPath, std::string const& locationPath)
{
	if (locationPath.empty())
		return false;
	if (requestPath == locationPath)
		return true;
	if (requestPath.find(locationPath) != 0)
		return false;
	if (locationPath[locationPath.size() - 1] == '/')
		return true;
	if (requestPath.size() > locationPath.size()
		&& requestPath[locationPath.size()] == '/')
		return true;
	return false;
}
//construit le path
static bool buildFilePath(std::string const& root, std::string const& path, Location const* location, ServerConfig const& server, std::string& filePath)
{
    std::string relativePath = path;
	std::string normalizedPath;

    if (location && locationMatches(path, location->path))
		relativePath = path.substr(location->path.size());
    if (relativePath.empty() || relativePath == "/")
    {
        if (location && !location->index.empty())
            relativePath = "/" + location->index;
        else if (!server.index.empty())
            relativePath = "/" + server.index;
        else
            relativePath = "/index.html";
    }
	if (!normalizeRelativePath(relativePath, normalizedPath))
		return false;
	if (!root.empty() && root[root.size() - 1] == '/')
		filePath = root.substr(0, root.size() - 1) + normalizedPath;
	else
		filePath = root + normalizedPath;
    return true;
}

// TODO:
// Cette version normalise les segments "." et ".." et bloque les remontées
// au-dessus de la racine logique.
//
// Amélioration possible plus tard:
// - utiliser realpath() pour une canonisation filesystem réelle
// - gérer les symlinks
// - gérer le décodage URL (%2e%2e, etc.)

//lire le fichier ce trouvant sur le path crée
static bool readFileContent(std::string const& filePath, std::string& content)
{
	std::ifstream file(filePath.c_str());

	if (!file.is_open())
		return false;
	std::ostringstream buffer;
	buffer << file.rdbuf();
	content = buffer.str();
	return true;
}
//ecrit dans un fichier
static bool writeFileContent(std::string const& filePath, std::string const& content)
{
	std::ofstream file(filePath.c_str());

	if (!file.is_open())
		return false;
	file << content;
	return true;
}
//outil delete
static bool deleteFile(std::string const& filePath)
{
	return (std::remove(filePath.c_str()) == 0);
}
//recuperer extension
static std::string getFileExtension(std::string const& filePath)
{
	size_t dotPos = filePath.rfind('.');
	if (dotPos == std::string::npos)
		return "";
	return filePath.substr(dotPos);
}
//recuperer l'extension
static std::string getContentType(std::string const& filePath)
{
	std::string extension = getFileExtension(filePath);
	if (extension == ".html" || extension == ".htm")
		return "text/html";
	if (extension == ".css")
		return "text/css";
	if (extension == ".js")
		return "application/javascript";
	if (extension == ".jpeg" || extension == ".jpg")
		return "image/jpeg";
	if (extension == ".png")
		return "image/png";
	if (extension == ".gif")
		return "image/gif";
	if (extension == ".txt")
		return "text/plain";
	return "application/octet-stream";
}
static std::string resolveUploadBase(ServerConfig const& server, Location const* location)
{
	if (location && !location->upload_dir.empty())
		return location->upload_dir;
	return resolveRoot(server, location);
}
static std::string buildUploadPath(ServerConfig const& server, Location const* location)
{
	std::string base = resolveUploadBase(server, location);
	if (!base.empty() && base[base.size() - 1] == '/')
		return base + "upload.txt";
	return base + "/upload.txt";
}
//verifie si le chemin existe
static bool pathExists(std::string const& path)
{
	struct stat pathStat;
	return (stat(path.c_str(), &pathStat) == 0);
}
//verifie si s'est un dossier
static bool isDirectory(std::string const& path)
{
	struct stat pathStat;
	if (stat(path.c_str(), &pathStat) != 0)
		return false;
	return S_ISDIR(pathStat.st_mode);
}
//recuperation de l'index
static std::string resolveIndex(ServerConfig const& server, Location const* location)
{
	if (location && !location->index.empty())
		return location->index;
	if (!server.index.empty())
		return server.index;
	return "index.html";
}
//colle le chemin et le nom du ficher
static std::string joinPath(std::string const& base, std::string const& extra)
{
	if (base.empty())
		return extra;
	if (base[base.size() - 1] == '/')
		return base + extra;
	return base + "/" + extra;
}
//si autoindex est permit
static bool isAutoIndexEnabled(Location const* location)
{
	if (!location)
		return false;
	return location->show_directory;
}
//une page html simple avec la liste des dossier
static bool buildDirectoryListing(std::string const& dirPath,
	std::string const& path, std::string& html)
{
	DIR* dir;
	struct dirent* entry;

	dir = opendir(dirPath.c_str());
	if (!dir)
		return false;
	html = "<html><body><h1>Index of " + path + "</h1><ul>";
	entry = readdir(dir);
	while (entry)
	{
		std::string name = entry->d_name;

		if (name != "." && name != "..")
		{
			html += "<li><a href=\"";
			if (!path.empty() && path[path.size() - 1] == '/')
				html += path + name;
			else
				html += path + "/" + name;
			html += "\">" + name + "</a></li>";
		}
		entry = readdir(dir);
	}
	html += "</ul></body></html>";
	closedir(dir);
	return true;
}
//verifie si redirection
static bool hasRedirect(Location const* location)
{
	if (!location)
		return false;
	return (location->redirect_page.first != 0
			&& !location->redirect_page.second.empty());
}
// check que le code est bon
static int resolveRedirectCode(Location const* location)
{
	int code = location->redirect_page.first;
	if (code == 301 || code == 302 || code == 303
		|| code == 307 || code == 308)
		return code;
	return 302;
}
//mets la bonne raison celon le bon code
static std::string getReasonPhrase(int code)
{
	switch (code)
	{
	case 200: return "OK";
	case 201: return "Created";
	case 301: return "Moved Permanently";
	case 302: return "Found";
	case 303: return "See Other";
	case 307: return "Temporary Redirect";
	case 308: return "Permanent Redirect";
	case 400: return "Bad Request";
	case 403: return "Forbidden";
	case 404: return "Not Found";
	case 405: return "Method Not Allowed";
	case 413: return "Payload Too Large";
	case 500: return "Internal Server Error";
	case 501: return "Not Implemented";
	case 505: return "HTTP Version Not Supported";
	default: return "Internal Server Error";
	}
}
static std::string intToString(int n)
{
	std::ostringstream oss;
	oss << n;
	return oss.str();
}
static std::string buildErrorBody(int code, std::string const& path, bool directoryListingDenied)
{
	std::string body = intToString(code) + " " + getReasonPhrase(code) + "\n";
	if (path.empty())
		return body;
	if (directoryListingDenied)
		return body + "directory listing denied: " + path + "\n";
	return body + "file path = " + path + "\n";
}
static bool getErrorPagePath(ServerConfig const& server, int code, std::string& path)
{
	std::map<int, std::string>::const_iterator it (server.error_pages.find(code));
	if (it == server.error_pages.end())
		return false;
	path = it->second;
	return true;
}
static HttpResponse buildErrorResponse(ServerConfig const& server, int code, std::string const& path = "", bool directoryListingDenied = false)
{
	HttpResponse response;
	std::string errorPath;
	std::string errorContent;

	response.statusCode = code;
	response.reasonPhrase = getReasonPhrase(code);
	if (getErrorPagePath(server, code, errorPath)
		&& readFileContent(errorPath, errorContent))
	{
		response.headers["Content-Type"] = getContentType(errorPath);
		response.body = errorContent;
		return response;
	}
	response.headers["Content-Type"] = "text/plain";
	response.body = buildErrorBody(code, path, directoryListingDenied);
	return response;
}
//detection cgi autoriser
static bool isCgiRequest(std::string const& filePath, Location const* location)
{
	std::string extension;
	if (!location)
		return false;
	extension = getFileExtension(filePath);
	if (extension.empty())
		return false;
	return (location->cgi_extensions.find(extension) != location->cgi_extensions.end());
}
static std::string getCgiInterpreter(std::string const& extension, Location const* location)
{
	if (!location)
		return "";
	std::map<std::string, std::string>::const_iterator it = location->cgi_interpreters.find(extension);
	if (it == location->cgi_interpreters.end())
		return "";
	return it->second;
}

//on fait une validation et on met les code d'erreur et les message d'erreur
HttpResponse RequestHandler::handleRequest(HttpRequest const& request, ServerConfig const& server, Location const* location)
{
    HttpResponse response;
	std::string root = resolveRoot(server, location);

	//si pas bonne version
    if (request.version != "HTTP/1.1")
        return buildErrorResponse(server, 505);

	//si s'est pas une method que nous supportons
    if (!isSupportedMethod(request.method))
		return buildErrorResponse(server, 501);

	//si s'est pas une methode que la location permet
	if (!isMethodAllowed(request.method, location))
		return buildErrorResponse(server, 405);

	//si le body depasse la taille limite
	if (!isBodySizeValid(request, server))
		return buildErrorResponse(server, 413);

	//si une redirection
	if (hasRedirect(location))
	{
		int code = resolveRedirectCode(location);

		response.statusCode = code;
		response.reasonPhrase = getReasonPhrase(code);
		response.headers["Location"] = location->redirect_page.second;
		response.headers["Content-Type"] = "text/plain";
		response.body = "Redirecting to: " + location->redirect_page.second + "\n";
		return response;
	}

	//et la on va faire les reponse de chaque methode
	if (request.method == "GET")
	{
		std::string fileContent;
		std::string filePath;

		if (!buildFilePath(root, request.path, location, server, filePath))
			return buildErrorResponse(server, 403, request.path);
		if (isCgiRequest(filePath, location))
		{
			std::string extension = getFileExtension(filePath);
			std::string interpreter = getCgiInterpreter(extension, location);

			if (interpreter.empty())
				return buildErrorResponse(server, 500, filePath);

			response.statusCode = 200;
			response.reasonPhrase = getReasonPhrase(200);
			response.headers["Content-Type"] = "text/plain";
			response.body = "CGI detected\n";
			response.body += "script: " + filePath + "\n";
			response.body += "extension: " + extension + "\n";
			response.body += "interpreter: " + interpreter + "\n";
			return response;
		}
		if (isDirectory(filePath))
		{
			std::string indexPath = joinPath(filePath, resolveIndex(server, location));

			if (readFileContent(indexPath, fileContent))
			{
				response.statusCode = 200;
				response.reasonPhrase = getReasonPhrase(200);
				response.headers["Content-Type"] = getContentType(indexPath);
				response.body = fileContent;
				return response;
			}
			if (isAutoIndexEnabled(location)
				&& buildDirectoryListing(filePath, request.path, fileContent))
			{
				response.statusCode = 200;
				response.reasonPhrase = getReasonPhrase(200);
				response.headers["Content-Type"] = "text/html";
				response.body = fileContent;
				return response;
			}
			return buildErrorResponse(server, 403, filePath, true);
		}
		if (!pathExists(filePath))
			return buildErrorResponse(server, 404, filePath);
		if (!readFileContent(filePath, fileContent))
			return buildErrorResponse(server, 403, filePath);

		response.statusCode = 200;
		response.reasonPhrase = getReasonPhrase(200);
		response.headers["Content-Type"] = getContentType(filePath);
		response.body = fileContent;
		return response;
	}

	if (request.method == "POST")
	{
		std::string filePath = buildUploadPath(server, location);

		if (!writeFileContent(filePath, request.body))
			return buildErrorResponse(server, 500, filePath);

		response.statusCode = 201;
		response.reasonPhrase = "Created";
		response.headers["Content-Type"] = "text/plain";
		response.body = "File created at: " + filePath + "\n";
		return response;
	}

	// TODO:
// Le fichier d'upload utilise encore un nom fixe "upload.txt".
// Améliorations possibles:
// - générer un nom unique
// - utiliser un nom fourni par la requête si vous l’implémentez plus tard
// - éviter que plusieurs POST écrasent le même fichier

	if (request.method == "DELETE")
	{
		std::string filePath;

		if (!buildFilePath(root, request.path, location, server, filePath))
			return buildErrorResponse(server, 403, request.path);
		if (!pathExists(filePath))
			return buildErrorResponse(server, 404, filePath);
		if (!deleteFile(filePath))
			return buildErrorResponse(server, 403, filePath);
		response.statusCode = 200;
		response.reasonPhrase = "OK";
		response.headers["Content-Type"] = "text/plain";
		response.body = "File deleted: " + filePath + "\n";
		return response;
	}
	return buildErrorResponse(server, 500);
}

// TODO:
// Cette fonction n'utilise pas encore pleinement la configuration issue de ServerConfig et Location.
//
// Problèmes actuels:
//
// 5. CGI:
//    Les fichiers CGI sont détectés,
//    mais ne sont pas encore exécutés.
//    Il faut encore lancer l'interpréteur avec fork/execve
//    et récupérer la sortie du script.
//
// Conclusion:
//    La config est bien parsée mais pas encore réellement appliquée ici.
//nico
