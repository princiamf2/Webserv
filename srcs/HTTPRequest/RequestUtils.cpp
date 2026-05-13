#include "RequestUtils.hpp"
#include <sstream>
#include <fstream>
#include <vector>
#include <sys/stat.h>


static bool locationMatchesUtil(std::string const& requestPath,
	std::string const& locationPath)
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

static bool normalizeRelativePathUtil(std::string const& rawPath,
	std::string& normalized)
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

static bool readFileContentUtil(std::string const& filePath, std::string& content)
{
	std::ifstream file(filePath.c_str());

	if (!file.is_open())
		return false;
	std::ostringstream buffer;
	buffer << file.rdbuf();
	content = buffer.str();
	return true;
}

static std::string getReasonPhraseUtil(int code)
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

static std::string intToStringUtil(int n)
{
	std::ostringstream oss;
	oss << n;
	return oss.str();
}

static std::string getContentTypeUtil(std::string const& filePath)
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

static std::string buildErrorBodyUtil(int code, std::string const& path,
	bool directoryListingDenied)
{
	std::string body = intToStringUtil(code) + " " + getReasonPhraseUtil(code) + "\n";

	if (path.empty())
		return body;
	if (directoryListingDenied)
		return body + "directory listing denied: " + path + "\n";
	return body + "file path = " + path + "\n";
}

static bool getErrorPagePathUtil(ServerConfig const& server, int code,
	std::string& path)
{
	std::map<int, std::string>::const_iterator it(server.error_pages.find(code));
	std::string root;
	std::string page;

	if (it == server.error_pages.end())
		return false;
	root = server.root;
	page = it->second;
	if (!root.empty() && root[root.size() - 1] == '/')
		root.erase(root.size() - 1);
	if (!page.empty() && page[0] == '/')
		path = root + page;
	else
		path = root + "/" + page;
	return true;
}

std::string trim(const std::string& str)
{
    size_t start = 0;
    size_t end = str.size();

    while (start < str.size() && (str[start] == ' ' || str[start] == '\t' || str[start] == '\r'))
        start++;
    while (end > start && (str[end - 1] == ' ' || str[end - 1] == '\t' || str[end - 1] == '\r'))
        end--;
    return str.substr(start, end - start);
}

std::string resolveRoot(ServerConfig const& server, Location const* location)
{
	if (location && !location->root.empty())
		return location->root;
	return server.root;
}

bool buildFilePath(std::string const& root,
	std::string const& path,
	Location const* location,
	ServerConfig const& server,
	std::string& filePath)
{
	std::string relativePath = path;
	std::string normalizedPath;

	(void)server;
	if (location && locationMatchesUtil(path, location->path))
		relativePath = path.substr(location->path.size());
	if (relativePath.empty())
		relativePath = "/";

	if (!normalizeRelativePathUtil(relativePath, normalizedPath))
		return false;
	if (!root.empty() && root[root.size() - 1] == '/')
		filePath = root.substr(0, root.size() - 1) + normalizedPath;
	else
		filePath = root + normalizedPath;
	return true;
}

std::string getFileExtension(std::string const& filePath)
{
	size_t dotPos = filePath.rfind('.');

	if (dotPos == std::string::npos)
		return "";
	return filePath.substr(dotPos);
}

HttpResponse buildErrorResponse(ServerConfig const& server, int code,
	std::string const& path, bool directoryListingDenied)
{
	HttpResponse response;
	std::string errorPath;
	std::string errorContent;

	response.statusCode = code;
	response.reasonPhrase = getReasonPhraseUtil(code);
	if (getErrorPagePathUtil(server, code, errorPath)
		&& readFileContentUtil(errorPath, errorContent))
	{
		response.headers["Content-Type"] = getContentTypeUtil(errorPath);
		response.body = errorContent;
		return response;
	}
	response.headers["Content-Type"] = "text/plain";
	response.body = buildErrorBodyUtil(code, path, directoryListingDenied);
	return response;
}
