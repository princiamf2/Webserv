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
//recuperer l'extention
static std::string getContentType(std::string const& filePath)
{
	size_t dotPos = filePath.rfind('.');

	if (dotPos == std::string::npos)
		return "text/plain";

	std::string extention = filePath.substr(dotPos);
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
	case 200: return "Ok";
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
	case 505: return "HTTP Version Not Suppoted";
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

		if (isDirectory(filePath))
		{
			std::string indexPath = joinPath(filePath, resolveIndex(server, location));

			if (readFileContent(indexPath, fileContent))
				return buildErrorResponse(server, 200);
			if (isAutoIndexEnabled(location)
				&& buildDirectoryListing(filePath, request.uri, fileContent))
				return buildErrorResponse(server, 200);
			return buildErrorResponse(server, 403, filePath, true);
		}
		if (!pathExists(filePath))
			return buildErrorResponse(server, 404, filePath);
		if (!readFileContent(filePath, fileContent))
			return buildErrorResponse(server, 403, filePath);
		return buildErrorResponse(server, 200);
	}

	if (request.method == "POST")
	{
		std::string filePath = buildUploadPath(server, location);

		if (!writeFileContent(filePath, request.body))
		{
			response.statusCode = 500;
			response.reasonPhrase = "Internal Server Error";
			response.headers["Content-Type"] = "text/plain";
			response.body = "500 Failed to write file\n";
			return response;
		}

		response.statusCode = 201;
		response.reasonPhrase = "Created";
		response.headers["Content-Type"] = "text/plain";
		response.body = "File created at: " + filePath + "\n";
		return response;
	}
	if (request.method == "DELETE")
	{
		std::string filePath = buildFilePath(root, request.uri, location, server);

		if (!deleteFile(filePath))
		{
			response.statusCode = 404;
			response.reasonPhrase = "Not Found";
			response.headers["Content-Type"] = "text/plain";
			response.body = "404 Not Found\n";
			response.body += "file path = " + filePath + "\n";
			return response;
		}
		response.statusCode = 200;
		response.reasonPhrase = "OK";
		response.headers["Content-Type"] = "text/plain";
		response.body = "File deleted: " + filePath + "\n";
		return response;
	}
	response.statusCode = 500;
	response.reasonPhrase = "Internal Server Error";
	response.headers["Content-Type"] = "text/plain";
	response.body = "500 Internal Server Error\n";
	return response;
}
