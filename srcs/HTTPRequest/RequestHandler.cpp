#include "RequestHandler.hpp"
#include "CgiManager.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "RequestAction.hpp"
#include "RequestUtils.hpp"
#include <cstddef>
#include <locale>
#include <set>
#include <sstream>
#include <fstream>
#include <string>
#include <cstdio>
#include <sys/stat.h>
#include <dirent.h>
#include <ctime>
#include <cstdlib>

struct SessionData
{
	int views;
	time_t createdAt;
	time_t lastSeen;

	SessionData() : views(0), createdAt(0), lastSeen(0) {}
};

static std::map<std::string, SessionData> g_sessions;

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
	file.write(content.data(), content.size());
	return file.good();
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
	if (extension == ".ico")
		return "image/x-icon";
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
		case 408: return "Request Timeout";
		case 413: return "Payload Too Large";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 504: return "Gateway Timeout";
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
static std::string extensionFromContentType(HttpRequest const& request)
{
    std::map<std::string, std::string>::const_iterator it =
        request.headers.find("content-type");

    if (it == request.headers.end())
        return ".bin";

    if (it->second.find("image/png") != std::string::npos)
        return ".png";
    if (it->second.find("image/jpeg") != std::string::npos)
        return ".jpeg";
    if (it->second.find("image/gif") != std::string::npos)
        return ".gif";
    if (it->second.find("text/plain") != std::string::npos)
        return ".txt";

    return ".bin";
}
static std::string extensionFromFileName(std::string const& filename)
{
	size_t pos = filename.find_last_of('.');

	if (pos == std::string::npos)
		return ".bin";
	return filename.substr(pos);
}
static std::string buildUniqueUploadPathWithExt(ServerConfig const& server, Location const* location, std::string const& ext)
{
	std::string base = resolveUploadBase(server, location);
	std::string candidate;
	int i = 0;

	while (true)
	{
		if (!base.empty() && base[base.size() - 1] == '/')
			candidate = base + "upload_" + intToString(i) + ext;
		else
			candidate = base + "/upload_" + intToString(i) + ext;
		if (!pathExists(candidate))
			return candidate;
		++i;
	}
}
static std::string buildUniqueUploadPath(ServerConfig const& server,
	Location const* location, HttpRequest const& request)
{
	std::string base = resolveUploadBase(server, location);
	std::string ext = extensionFromContentType(request);
	std::string candidate;
	int i = 0;

	while (true)
	{
		if (!base.empty() && base[base.size() - 1] == '/')
			candidate = base + "upload_" + intToString(i) + ext;
		else
			candidate = base + "/upload_" + intToString(i) + ext;
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
		allow += "GET";
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
static std::string generateSessionId()
{
	static unsigned long counter = 0;
	std::ostringstream oss;

	++counter;
	oss << std::hex
		<< std::time(NULL)
		<< "_"
		<< counter
		<< "_"
		<< std::rand();
	return oss.str();
}
static std::string getCookieValue(HttpRequest const& request, std::string const& name)
{
	std::map<std::string, std::string>::const_iterator it;
	std::string cookies;
	size_t pos;

	it = request.headers.find("cookie");
	if (it == request.headers.end())
		return "";
	cookies = it->second;
	pos = 0;
	while (pos < cookies.size())
	{
		size_t end = cookies.find(';', pos);
		std::string part;
		if (end == std::string::npos)
			end = cookies.size();
		part = cookies.substr(pos, end - pos);
		part = trim(part);
		size_t eq = part.find('=');
		if (eq != std::string::npos)
		{
			std::string key = trim(part.substr(0, eq));
			std::string value = trim(part.substr(eq + 1));
			if (key == name)
				return value;
		}
		pos = end + 1;
	}
	return "";
}

static std::string ensureSession(HttpRequest const& request, HttpResponse& response)
{
	std::string sid;
	time_t now;

	now = std::time(NULL);
	sid = getCookieValue(request, "session_id");
	if (sid.empty() || g_sessions.find(sid) == g_sessions.end())
	{
		sid = generateSessionId();
		SessionData data;
		data.createdAt = now;
		data.lastSeen = now;
		data.views = 0;
		g_sessions[sid] = data;

		response.headers["Set-Cookie"] = "session_id=" + sid + "; Path=/; HttpOnly; SameSite=Lax";
	}
	g_sessions[sid].views++;
	g_sessions[sid].lastSeen = now;
	return sid;
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
		std::string sid = ensureSession(request, response);

		if (request.path == "/session")
		{
			std::ostringstream body;
			SessionData const& data = g_sessions[sid];

			body << "Session ID: " << sid << "\n";
			body << "Views: " << data.views << "\n";
			body << "Created at: " << data.createdAt << "\n";
			body << "Last seen: " << data.lastSeen << "\n";

			response.statusCode = 200;
			response.reasonPhrase = getReasonPhrase(200);
			response.headers["Content-Type"] = "text/plain";
			response.body = body.str();
			applyHeadLogic(response, request);
			return response;
		}

		if (!buildFilePath(root, request.path, location, server, filePath))
			return buildErrorResponse(server, 400, request.path);

		if (isDirectory(filePath))
		{
			std::string indexPath = joinPath(filePath,
				resolveIndex(server, location));
			bool indexExists = pathExists(indexPath);
			//strict chmod 000 bug
			//if (indexExists && !readFileContent(indexPath, fileContent))
			//	return buildErrorResponse(server, 403, indexPath);

			if (indexExists && readFileContent(indexPath, fileContent))
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
			if (!indexExists)
				return buildErrorResponse(server, 403, indexPath);
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
		std::string uploadBody;

		if (!buildFilePath(root, request.path, location, server, filePath))
			return buildErrorResponse(server, 400, request.path);

		if (request.isMultipart)
		{
			uploadBody = request.uploadContent;
			std::string urlFile = extractFileName(request.path);
			if (!request.path.empty() && request.path[request.path.size() - 1] != '/' && !urlFile.empty())
			{
				std::string uploadBase = resolveUploadBase(server, location);
				if (!uploadBase.empty() && uploadBase[uploadBase.size() - 1] == '/')
					filePath = uploadBase + urlFile;
				else
					filePath = uploadBase + "/" + urlFile;
			}
			else
				filePath = buildUniqueUploadPathWithExt(server, location, extensionFromFileName(request.uploadFilename));
		}
		else
		{
			uploadBody = request.body;
			std::string urlFile = extractFileName(request.path);
			if (!request.path.empty() && request.path[request.path.size() - 1] != '/' && !urlFile.empty())
			{
				std::string uploadBase = resolveUploadBase(server, location);
				if (!uploadBase.empty() && uploadBase[uploadBase.size() - 1] == '/')
					filePath = uploadBase + urlFile;
				else
					filePath = uploadBase + "/" + urlFile;
			}
			else
				filePath = buildUniqueUploadPath(server, location, request);
		}
		if (!writeFileContent(filePath, uploadBody))
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
		response.body = "File created at: " + locationHeader + "\n";
		return response;
	}

	if (request.method == "DELETE")
	{
		std::string deleteBase;
		std::string filePath;

		deleteBase = resolveUploadBase(server, location);
		if (!buildFilePath(deleteBase, request.path, location, server, filePath))
			return buildErrorResponse(server, 400, request.path);
		if (!pathExists(filePath))
			return buildErrorResponse(server, 404, filePath);
		if (!deleteFile(filePath))
			return buildErrorResponse(server, 403, filePath);
		response.statusCode = 204;
		response.reasonPhrase = "No Content";
		// sur 204, Content-Type pas necessaire, le garder reste tolere par les clients.
		response.headers["Content-Type"] = "text/plain";
		response.body.clear();
		return response;
	}
	return buildErrorResponse(server, 500);
}

static bool findCgiScriptInPath(std::string const& requestPath, Location const* location,
	std::string& scriptName, std::string& pathInfo)
{
	size_t bestPos = std::string::npos;
	std::string bestExt;

	if (!location)
		return false;
	for (std::set<std::string>::const_iterator it = location->cgi_extensions.begin();
		it != location->cgi_extensions.end(); ++it)
	{
		size_t pos = requestPath.find(*it);
		if (pos == std::string::npos)
			continue;
		size_t end = pos + it->size();
		if (end < requestPath.size() && requestPath[end] != '/')
			continue;
		if (bestPos == std::string::npos || end < bestPos + bestExt.size())
		{
			bestPos = pos;
			bestExt = *it;
		}
	}
	if (bestPos == std::string::npos)
		return false;

	size_t scriptEnd = bestPos + bestExt.size();
	scriptName = requestPath.substr(0, scriptEnd);
	pathInfo = requestPath.substr(scriptEnd);
	return true;
}

ActionRequest RequestHandler::resolveAction(HttpRequest const& request,
	ServerConfig const& server, Location const* location)
{
	ActionRequest action;
	std::string root;
	std::string filePath;
	std::string extension;
	std::string scriptName;
	std::string pathInfo;


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
		&& findCgiScriptInPath(request.path, location, scriptName, pathInfo)
		&& buildFilePath(root, scriptName, location, server, filePath)
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
		
		action.type = ACTION_START_CGI;
		action.request = request;
		action.location = location;
		action.scriptPath = filePath;
		action.scriptName = scriptName;
		action.pathInfo = pathInfo;
		return action;
	}
	action.type = ACTION_IMMEDIATE_RESPONSE;
	action.response = handleRequest(request, server, location);
	return action;
}
