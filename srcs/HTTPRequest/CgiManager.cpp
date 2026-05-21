/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiManager.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Nicolsan <nicolas.sanchezroca123@gmail.    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/09 14:52:49 by malapoug          #+#    #+#             */
/*   Updated: 2026/05/21 10:23:49 by Nicolsan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "CgiManager.hpp"
#include "../Core/webserv.hpp"

#include <cctype>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <map>
#include <cstddef>
#include <fcntl.h>

// canonic
CgiManager::CgiManager() {}
CgiManager::CgiManager(CgiManager const& other) {(void)other;}
CgiManager& CgiManager::operator=(CgiManager const& other) {(void)other; return *this;}
CgiManager::~CgiManager() {}

// helper local : phrase HTTP selon code
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

static std::string getFileExtensionLocal(const std::string& filePath)
{
	size_t dotPos = filePath.rfind('.');
	if (dotPos == std::string::npos)
		return "";
	return filePath.substr(dotPos);
}

static std::string intToStringLocal(int n)
{
	std::ostringstream oss;
	oss << n;
	return oss.str();
}

bool CgiManager::isCgiRequest(const std::string& filePath,
	const Location* location)
{
	std::string extension;

	if (!location)
		return false;
	extension = getFileExtensionLocal(filePath);
	if (extension.empty())
		return false;
	return (location->cgi_extensions.find(extension)
		!= location->cgi_extensions.end());
}

std::string CgiManager::getCgiInterpreter(const std::string& extension,
	const Location* location)
{
	std::map<std::string, std::string>::const_iterator it;

	if (!location)
		return "";
	it = location->cgi_interpreters.find(extension);
	if (it == location->cgi_interpreters.end())
		return "";
	return it->second;
}

std::string CgiManager::getScriptName(const std::string& scriptPath)
{
	size_t slashPos = scriptPath.find_last_of('/');

	if (slashPos == std::string::npos)
		return scriptPath;
	return scriptPath.substr(slashPos + 1);
}

std::string CgiManager::getDirectoryPath(const std::string& path)
{
	size_t slashPos = path.find_last_of('/');

	if (slashPos == std::string::npos)
		return ".";
	if (slashPos == 0)
		return "/";
	return path.substr(0, slashPos);
}

static std::string toCgiHeaderName(std::string key)
{
	for (size_t i = 0; i < key.size(); ++i)
	{
		if (key[i] == '-')
			key[i] = '_';
		else
			key[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(key[i])));
	}
	return "HTTP_" + key;
}

char** CgiManager::buildCgiEnv(const HttpRequest& request,
	const ServerConfig& server,
	const Location* location, const std::string& scriptPath,
	const std::string& scriptName, const std::string& pathInfo)
{
	(void)location;
	std::vector<std::string> envStrings;
	char** env;
	std::string contentLength;
	std::map<std::string, std::string>::const_iterator it;

	contentLength = intToStringLocal(static_cast<int>(request.body.size()));

	envStrings.push_back("REQUEST_METHOD=" + request.method);
	envStrings.push_back("QUERY_STRING=" + request.query);
	envStrings.push_back("SCRIPT_NAME=" + scriptName);
	envStrings.push_back("SCRIPT_FILENAME=" + scriptPath);
	envStrings.push_back("PATH_INFO=" + pathInfo);
	envStrings.push_back("REQUEST_URI=" + request.uri);
	envStrings.push_back("CONTENT_LENGTH=" + contentLength);
	envStrings.push_back("DOCUMENT_ROOT=" + server.root);
	envStrings.push_back("GATEWAY_INTERFACE=CGI/1.1");
	envStrings.push_back("SERVER_PROTOCOL=" + request.version);
	envStrings.push_back("REDIRECT_STATUS=200");
	envStrings.push_back("SERVER_NAME=webserv");
	it = request.headers.find("host");
	if (it != request.headers.end())
	{
		std::string hostValue = it->second;
		size_t colonPos = hostValue.rfind(':');

		if (colonPos != std::string::npos)
			envStrings.push_back("SERVER_PORT=" + hostValue.substr(colonPos + 1));
		else
			envStrings.push_back("SERVER_PORT=80");
	}
	else
		envStrings.push_back("SERVER_PORT=80");

	it = request.headers.find("content-type");
	if (it != request.headers.end())
		envStrings.push_back("CONTENT_TYPE=" + it->second);
	else
		envStrings.push_back("CONTENT_TYPE=");

	it = request.headers.find("host");
	if (it != request.headers.end())
		envStrings.push_back("HTTP_HOST=" + it->second);
	else
		envStrings.push_back("HTTP_HOST=");
	for (std::map<std::string, std::string>::const_iterator hit = request.headers.begin();
		hit != request.headers.end(); ++hit)
	{
		if (hit->first == "content-type" || hit->first == "content-length")
			continue;
		if (hit->first == "host")
			continue;
		if (hit->first == "transfer-encoding")
			continue;
		envStrings.push_back(toCgiHeaderName(hit->first) + "=" + hit->second);
	}

	env = new char*[envStrings.size() + 1];
	for (size_t i = 0; i < envStrings.size(); ++i)
	{
		env[i] = new char[envStrings[i].size() + 1];
		std::strcpy(env[i], envStrings[i].c_str());
	}
	env[envStrings.size()] = NULL;
	return env;
}

void CgiManager::freeCgiEnv(char** envp)
{
	size_t i = 0;

	if (!envp)
		return;
	while (envp[i])
	{
		delete[] envp[i];
		++i;
	}
	delete[] envp;
}

std::string CgiManager::trim(const std::string& s)
{
	size_t start = 0;
	size_t end = s.size();

	while (start < s.size()
		&& (s[start] == ' ' || s[start] == '\t' || s[start] == '\r'))
		++start;
	while (end > start
		&& (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r'))
		--end;
	return s.substr(start, end - start);
}

int CgiManager::parseCgiStatusCode(const std::string& value)
{
	std::istringstream iss(value);
	int code;

	if (!(iss >> code))
		return 200;
	if (code < 100 || code > 599)
		return 200;
	return code;
}

static std::string normalizeHeaderKey(const std::string& key)
{
	std::string lower = key;
	for (size_t i = 0; i < lower.size(); ++i)
		lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[i])));

	if (lower == "content-type")
		return "Content-Type";
	if (lower == "content-length")
		return "Content-Length";
	if (lower == "location")
		return "Location";
	if (lower == "status")
		return "Status";
	return key;
}

HttpResponse CgiManager::buildResponseFromCgiOutput(const std::string& output)
{
	HttpResponse response;
	size_t separatorPos;
	size_t separatorLen;
	std::string headerPart;
	std::string bodyPart;
	std::istringstream headerStream;
	std::string line;

	response.statusCode = 200;
	response.reasonPhrase = getReasonPhrase(200);

	separatorPos = output.find("\r\n\r\n");
	separatorLen = 4;
	if (separatorPos == std::string::npos)
	{
		separatorPos = output.find("\n\n");
		separatorLen = 2;
	}

	if (separatorPos == std::string::npos)
	{
		response.headers["Content-Type"] = "text/plain";
		response.body = output;
		return response;
	}

	headerPart = output.substr(0, separatorPos);
	bodyPart = output.substr(separatorPos + separatorLen);

	headerStream.str(headerPart);
	while (std::getline(headerStream, line))
	{
		size_t colonPos;
		std::string key;
		std::string value;

		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		colonPos = line.find(':');
		if (colonPos == std::string::npos)
			continue;

		key = normalizeHeaderKey(trim(line.substr(0, colonPos)));
		value = trim(line.substr(colonPos + 1));

		if (key.empty())
			continue;

		if (key == "Status")
		{
			response.statusCode = parseCgiStatusCode(value);
			response.reasonPhrase = getReasonPhrase(response.statusCode);
		}
		else if (key == "Location")
		{
			response.headers[key] = value;
			if (response.statusCode == 200)
			{
				response.statusCode = 302;
				response.reasonPhrase = getReasonPhrase(response.statusCode);
			}
		}
		else
			response.headers[key] = value;
	}

	if (response.headers.find("Content-Type") == response.headers.end())
		response.headers["Content-Type"] = "text/plain";

	response.body = bodyPart;
	return response;
}

bool CgiManager::startProcess(CgiProcess& process,
	const HttpRequest& request,
	const ServerConfig& server,
	const Location* location,
	const std::string& scriptPath,
	const std::string& scriptName,
	const std::string& pathInfo,
	const std::string& interpreter)
{
	int inputPipe[2];
	int outputPipe[2];
	pid_t pid;
	char** envp;
	char* argv[3];
	std::string scriptDir;
	std::string executableName;
	std::string executablePath;

	if (pipe(inputPipe) == -1)
		return false;
	if (pipe(outputPipe) == -1)
	{
		close(inputPipe[0]);
		close(inputPipe[1]);
		return false;
	}

	pid = fork();
	if (pid < 0)
	{
		close(inputPipe[0]);
		close(inputPipe[1]);
		close(outputPipe[0]);
		close(outputPipe[1]);
		return false;
	}

	if (pid == 0)
	{
		if (dup2(inputPipe[0], STDIN_FILENO) == -1)
			std::exit(1);
		if (dup2(outputPipe[1], STDOUT_FILENO) == -1)
			std::exit(1);

		close(inputPipe[0]);
		close(inputPipe[1]);
		close(outputPipe[0]);
		close(outputPipe[1]);

		scriptDir = getDirectoryPath(scriptPath);
		executableName = getScriptName(scriptPath);
		executablePath = "./" + executableName;
		if (chdir(scriptDir.c_str()) == -1)
			std::exit(1);

		std::string cgiScriptFilename = scriptPath;
		if (!interpreter.empty())
			cgiScriptFilename = "./" + executableName;

		envp = buildCgiEnv(request, server, location, cgiScriptFilename, scriptName, pathInfo);

		if (!interpreter.empty())
		{
			argv[0] = const_cast<char*>(interpreter.c_str());
			argv[1] = const_cast<char*>(executableName.c_str());
			argv[2] = NULL;
		}
		else
		{
			argv[0] = const_cast<char*>(executablePath.c_str());
			argv[1] = NULL;
			argv[2] = NULL;
		}

		execve(argv[0], argv, envp);
		freeCgiEnv(envp);
		std::exit(1);
	}

	close(inputPipe[0]);
	close(outputPipe[1]);

	process.pid = pid;
	process.stdinFd = inputPipe[1];
	process.stdoutFd = outputPipe[0];
	fcntl(process.stdinFd, F_SETFL, fcntl(process.stdinFd, F_GETFL, 0) | O_NONBLOCK);
	fcntl(process.stdoutFd, F_SETFL, fcntl(process.stdoutFd, F_GETFL, 0) | O_NONBLOCK);
	process.inputBuffer = request.body;
	process.inputOffset = 0;
	process.outputBuffer.clear();
	process.stdinClosed = false;
	process.stdoutClosed = false;
	process.childExited = false;
	process.error = false;
	process.exitStatus = 0;
	process.startTime = time(NULL);
	logs("cgi process forked pid=" + toString((int)pid) + " script=" + scriptPath);
	return true;
}

bool CgiManager::writeInput(CgiProcess& process)
{
	if (process.stdinClosed)
		return true;

	if (process.inputOffset >= process.inputBuffer.size())
	{
		close(process.stdinFd);
		process.stdinFd = -1;
		process.stdinClosed = true;
		return true;
	}

	size_t remaining = process.inputBuffer.size() - process.inputOffset;
	size_t chunkSize = remaining;

	if (chunkSize > 4096)
		chunkSize = 4096;
	ssize_t written = write(process.stdinFd,
		process.inputBuffer.c_str() + process.inputOffset,
		chunkSize);

	//write peut retourner 0 (aucun byte) ou -1 (erreur), les deux ferment stdin
	if (written <= 0)
		return false;

	process.inputOffset += static_cast<size_t>(written);

	if (process.inputOffset >= process.inputBuffer.size())
	{
		close(process.stdinFd);
		process.stdinFd = -1;
		process.stdinClosed = true;
	}
	return true;
}

bool CgiManager::readOutput(CgiProcess& process)
{
	char buffer[1024];
	ssize_t bytesRead;

	if (process.stdoutClosed)
		return true;

	bytesRead = read(process.stdoutFd, buffer, sizeof(buffer));
	//read=0 (EOF), <0 (erreur) -> ferme stdout, >0 -> continue
	if (bytesRead < 0)
		return false;
	if (bytesRead == 0)
	{
		close(process.stdoutFd);
		process.stdoutFd = -1;
		process.stdoutClosed = true;
		return true;
	}
	process.outputBuffer.append(buffer, bytesRead);
	return true;
}

bool CgiManager::checkChild(CgiProcess& process)
{
	int status;
	pid_t ret;

	if (process.childExited)
		return true;
	ret = waitpid(process.pid, &status, WNOHANG);
	if (ret < 0)
		return false;
	if (ret == 0)
		return true;

	process.childExited = true;
	process.exitStatus = status;
	logs("cgi child exited pid=" + toString((int)process.pid) + " status=" + toString(status));
	return true;
}

CgiResult CgiManager::buildFinalResult(CgiProcess& process)
{
	CgiResult result;

	result.rawOutput = process.outputBuffer;

	if (process.error)
		return result;

	if (result.rawOutput.empty())
		return result;
	// verifie fin propre du CGI
	if (!WIFEXITED(process.exitStatus))
    	return result;
	if (WEXITSTATUS(process.exitStatus) != 0)
    	return result;

	result.response = buildResponseFromCgiOutput(result.rawOutput);
	result.success = true;
	return result;
}

void CgiManager::cleanupProcess(CgiProcess& process)
{
	if (process.stdinFd != -1)
		close(process.stdinFd);
	if (process.stdoutFd != -1)
		close(process.stdoutFd);
	process.stdinFd = -1;
	process.stdoutFd = -1;
	process.stdinClosed = true;
	process.stdoutClosed = true;
	process.error = false;
	process.timedOut = false;
}
