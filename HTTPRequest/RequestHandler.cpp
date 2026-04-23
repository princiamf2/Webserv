#include "RequestHandler.hpp"
#include "CgiManager.hpp"
#include "RequestAction.hpp"
#include "RequestUtils.hpp"
#include <sstream>
#include <fstream>
#include <string>
#include <cstdio>
#include <sys/stat.h>
#include <dirent.h>

//canonic
RequestHandler::RequestHandler() {}
RequestHandler::RequestHandler(RequestHandler const& other) {(void)other;}
RequestHandler& RequestHandler::operator=(RequestHandler const& other) {(void)other; return *this;}
RequestHandler::~RequestHandler() {}

//petit check de la taile du body
static bool isBodySizeValid(HttpRequest const& request, ServerConfig const& server)
{
	if (server.client_max_body_size == 0)
		return true;
	return (request.body.size() <= server.client_max_body_size);
}
//petit outil qui check les methods si la method on la supporte
static bool isSupportedMethod(std::string const& method)
{
	return (method == "GET" || method == "HEAD"
		|| method == "POST" || method == "DELETE");
}
//petit outil qui check si la methode et autoriser dans une location
static bool isMethodAllowed(std::string const& method, Location const* location)
{
	if (!location)
		return true;
	if (location->allowed_methods_http.empty())
		return true;
	if (method == "HEAD")
	{
		return (location->allowed_methods_http.find("HEAD")
				!= location->allowed_methods_http.end()
			|| location->allowed_methods_http.find("GET")
				!= location->allowed_methods_http.end());
	}
	return (location->allowed_methods_http.find(method)
		!= location->allowed_methods_http.end());
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
//recuperer le bon content type
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
static std::string buildUniqueUploadPath(ServerConfig const& server,
	Location const* location)
{
	std::string base = resolveUploadBase(server, location);
	std::string candidate;
	int i = 0;

	while (true)
	{
		if (!base.empty() && base[base.size() - 1] == '/')
			candidate = base + "upload_" + intToString(i) + ".txt";
		else
			candidate = base + "/upload_" + intToString(i) + ".txt";
		if (!pathExists(candidate))
			return candidate;
		++i;
	}
}
static std::string extractFileName(std::string const& path)
{
	size_t pos = path.find_last_of('/');

	if (pos == std::string::npos)
		return path;
	return path.substr(pos + 1);
}
//traitement de la method head
static void applyHeadLogic(HttpResponse& response, const HttpRequest& request)
{
	if (request.method != "HEAD")
		return;

	std::ostringstream oss;
	oss << response.body.size();
	response.headers["Content-Length"] = oss.str();
	response.body.clear();
}
//petit outil pour mettre dans le header de la reponse les method autoriser quand l'utilisateur rentre une method pas allow
static std::string buildAllowHeader(Location const* location)
{
	std::string allow;
	bool first = true;

	if (!location || location->allowed_methods_http.empty())
		return ("GET, HEAD, POST, DELETE");
	if (location->allowed_methods_http.find("GET")
		!= location->allowed_methods_http.end())
	{
		if (!first)
			allow += ", ";
		allow += "GET, HEAD";
		first = false;
	}
	if (location->allowed_methods_http.find("POST")
		!= location->allowed_methods_http.end())
	{
		if (!first)
			allow += ", ";
		allow += "POST";
		first = false;
	}
	if (location->allowed_methods_http.find("DELETE")
		!= location->allowed_methods_http.end())
	{
		if (!first)
			allow += ", ";
		allow += "DELETE";
		first = false;
	}
	return allow;
}
//constructions de la reponse d'une method non allow
static HttpResponse buildMethodNotAllowedResponse(ServerConfig const& server,
	Location const* location)
{
	HttpResponse response;

	response = buildErrorResponse(server, 405);
	response.headers["Allow"] = buildAllowHeader(location);
	return response;
}
//on fait une validation et on met les code d'erreur et les message d'erreur
HttpResponse RequestHandler::handleRequest(HttpRequest const& request,
	ServerConfig const& server, Location const* location)
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
		return buildMethodNotAllowedResponse(server, location);

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
		response.body = "Redirecting to: "
			+ location->redirect_page.second + "\n";
		return response;
	}

	//et la on va faire les reponse de chaque methode
	if (request.method == "GET" || request.method == "HEAD")
	{
		std::string fileContent;
		std::string filePath;

		if (!buildFilePath(root, request.path, location, server, filePath))
			return buildErrorResponse(server, 403, request.path);

		if (isDirectory(filePath))
		{
			std::string indexPath = joinPath(filePath,
				resolveIndex(server, location));

			if (readFileContent(indexPath, fileContent))
			{
				response.statusCode = 200;
				response.reasonPhrase = getReasonPhrase(200);
				response.headers["Content-Type"] = getContentType(indexPath);
				response.body = fileContent;
				applyHeadLogic(response, request);
				return response;
			}
			if (isAutoIndexEnabled(location)
				&& buildDirectoryListing(filePath, request.path, fileContent))
			{
				response.statusCode = 200;
				response.reasonPhrase = getReasonPhrase(200);
				response.headers["Content-Type"] = "text/html";
				response.body = fileContent;
				applyHeadLogic(response, request);
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
		applyHeadLogic(response, request);
		return response;
	}

	if (request.method == "POST")
	{
		std::string filePath;

		if (!buildFilePath(root, request.path, location, server, filePath))
			return buildErrorResponse(server, 403, request.path);

		filePath = buildUniqueUploadPath(server, location);
		if (!writeFileContent(filePath, request.body))
			return buildErrorResponse(server, 500, filePath);

		std::string fileName = extractFileName(filePath);
		std::string locationHeader;

		if (location && !location->path.empty())
		{
			if (location->path[location->path.size() - 1] == '/')
				locationHeader = location->path + fileName;
			else
				locationHeader = location->path + "/" + fileName;
		}
		else
			locationHeader = "/" + fileName;

		response.statusCode = 201;
		response.reasonPhrase = "Created";
		response.headers["Content-Type"] = "text/plain";
		response.headers["Location"] = locationHeader;
		response.headers["Set-Cookie"] = "last_upload="
			+ fileName + "; Path=/";
		response.body = "File created at: " + locationHeader + "\n";
		return response;
	}

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

ActionRequest RequestHandler::resolveAction(HttpRequest const& request,
	ServerConfig const& server, Location const* location)
{
	ActionRequest action;
	std::string root;
	std::string filePath;
	std::string extension;

	root = resolveRoot(server, location);

	if (request.version != "HTTP/1.1"
		|| !isSupportedMethod(request.method)
		|| !isMethodAllowed(request.method, location)
		|| !isBodySizeValid(request, server)
		|| hasRedirect(location))
	{
		action.type = ACTION_IMMEDIATE_RESPONSE;
		action.response = handleRequest(request, server, location);
		return action;
	}
	if ((request.method == "GET" || request.method == "HEAD"
			|| request.method == "POST")
		&& buildFilePath(root, request.path, location, server, filePath)
		&& CgiManager::isCgiRequest(filePath, location))
	{
		if (!pathExists(filePath))
		{
			action.type = ACTION_IMMEDIATE_RESPONSE;
			action.response = buildErrorResponse(server, 404, filePath);
			return action;
		}
		extension = getFileExtension(filePath);
		action.interpreter = CgiManager::getCgiInterpreter(extension, location);
		if (action.interpreter.empty())
		{
			action.type = ACTION_IMMEDIATE_RESPONSE;
			action.response = buildErrorResponse(server, 500, filePath);
			return action;
		}
		action.type = ACTION_START_CGI;
		action.request = request;
		action.location = location;
		action.scriptPath = filePath;
		return action;
	}
	action.type = ACTION_IMMEDIATE_RESPONSE;
	action.response = handleRequest(request, server, location);
	return action;
}
