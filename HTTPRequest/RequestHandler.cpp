#include "RequestHandler.hpp"
#include <sstream>
#include <fstream>
#include <string>
#include <cstdio>
#include <sys/stat.h>
#include <dirent.h>

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
//construit le path
static std::string buildFilePath(std::string const& root, std::string const& uri, Location const* location, ServerConfig const& server)
{
	std::string relativPath = uri;

	if (location && uri.find(location->path) == 0)
		relativPath = uri.substr(location->path.size());
	if (relativPath.empty() || relativPath == "/")
	{
		if (location && !location->index.empty())
			relativPath = "/" + location->index;
		else if (!server.index.empty())
			relativPath = "/" + server.index;
		else
			relativPath = "/index.html";
	}
	return root + relativPath;
}

// TODO:
// Cette fonction utilise directement l'URI pour construire un chemin filesystem,
// ce qui pose plusieurs problèmes critiques:
//
// 1. Query string:
//    L'URI peut contenir "?...", qui ne doit pas être utilisé pour accéder au fichier.
//    Il faut utiliser uniquement le path.
//
// 2. Sécurité:
//    Aucune protection contre les attaques de type "../" (directory traversal).
//    Exemple: GET /../../etc/passwd
//    Il faut normaliser ou refuser ce type de chemin.
//
// 3. Mapping direct URI -> filesystem:
//    Ce mapping est trop naïf.
//    Il faut s'assurer que le chemin final reste bien dans le root configuré.
//
// Sans ces protections, le serveur est vulnérable et incorrect.
//nico 


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
//recuperer extention
static std::string getFileExtension(std::string const& filePath)
{
	size_t dotPos = filePath.rfind('.');
	if (dotPos == std::string::npos)
		return "";
	return filePath.substr(dotPos);
}
//recuperer l'extention
static std::string getContentType(std::string const& filePath)
{
	std::string extention = getFileExtension(filePath);
	if (extention == ".html" || extention == ".htm")
		return "text/html";
	if (extention == ".css")
		return "text/css";
	if (extention == ".js")
		return "application/javascript";
	if (extention == ".jpeg" || extention == ".jpg")
		return "image/jpeg";
	if (extention == ".png")
		return "image/png";
	if (extention == ".gif")
		return "image/gif";
	if (extention == ".txt")
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
	std::string const& uri, std::string& html)
{
	DIR* dir;
	struct dirent* entry;

	dir = opendir(dirPath.c_str());
	if (!dir)
		return false;
	html = "<html><body><h1>Index of " + uri + "</h1><ul>";
	entry = readdir(dir);
	while (entry)
	{
		std::string name = entry->d_name;

		if (name != "." && name != "..")
		{
			html += "<li><a href=\"";
			if (!uri.empty() && uri[uri.size() - 1] == '/')
				html += uri + name;
			else
				html += uri + "/" + name;
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
std::string getReasonPhrase(int code)
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
static bool isCGIRequest(std::string const& filePath, Location const* location)
{
	std::string extension;
	if (!location)
		return false;
	extension = getFileExtension(filePath);
	if (extension.empty())
		return false;
	return (location->cgi_extensions.find(extension) != location->cgi_extensions.end());
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
		std::string filePath = buildFilePath(root, request.uri, location, server);

		if (isCGIRequest(filePath, location))
		{
			response.statusCode = 200;
			response.reasonPhrase = getReasonPhrase(200);
			response.headers["Content-Type"] = "text/plain";
			response.body = "CGI detected " + filePath + "\n";
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
				&& buildDirectoryListing(filePath, request.uri, fileContent))
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
// Le chemin de sortie est actuellement hardcodé:
//     root + "/upload.txt"
//
// Il faut:
//   - utiliser location->upload_dir si défini
//   - éventuellement utiliser un nom dynamique (ex: timestamp, nom envoyé, etc.)
//
// Sinon, toutes les requêtes POST écrasent le même fichier, ce qui est incorrect.
//nico

	if (request.method == "DELETE")
	{
		std::string filePath = buildFilePath(root, request.uri, location, server);

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
// 1. error_pages:
//    Les pages d'erreur configurées dans server.error_pages ne sont jamais utilisées.
//    Il faut servir les fichiers d'erreur personnalisés si définis.
//
// 2. upload_dir:
//    Les requêtes POST écrivent toujours dans "root/upload.txt".
//    Il faut utiliser location->upload_dir si défini.
//
// 3. redirect_page:
//    Les redirections configurées (code + URL) ne sont pas gérées.
//    Il faut détecter et retourner une réponse 3xx avec header Location.
//
// 4. show_directory:
//    Si aucun index n'est trouvé et show_directory == true,
//    il faut générer un listing du dossier.
//
// 5. cgi_extensions:
//    Les extensions CGI sont parsées mais jamais utilisées.
//    Il faudra détecter ces fichiers et les exécuter via CGI.
//
// 6. HTTP logique:
//    Certaines erreurs devraient être plus fines:
//      - 400 (Bad Request)
//      - 403 (Forbidden)
//      - 404 (Not Found)
//    Actuellement tout est simplifié.
//
// Conclusion:
//    La config est bien parsée mais pas encore réellement appliquée ici.
//nico
