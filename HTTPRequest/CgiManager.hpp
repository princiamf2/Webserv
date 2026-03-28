#ifndef CGIMANAGER_HPP
# define CGIMANAGER_HPP

# include <string>
# include "../Parsing/ServerConfig.hpp"
# include "../Parsing/Location.hpp"
# include "HttpRequest.hpp"
# include "HttpResponse.hpp"
# include "CgiTypes.hpp"
# include "CgiProcess.hpp"

class CgiManager
{
	public:
		static bool isCgiRequest(const std::string& filePath, const Location* location);
		static std::string getCgiInterpreter(const std::string& extension, const Location* location);

		static bool startProcess(CgiProcess& process,
			const HttpRequest& request,
			const ServerConfig& server,
			const Location* location,
			const std::string& scriptPath,
			const std::string& interpreter);

		static bool writeInput(CgiProcess& process);
		static bool readOutput(CgiProcess& process);
		static bool checkChild(CgiProcess& process);
		static CgiResult buildFinalResult(CgiProcess& process);
		static void cleanupProcess(CgiProcess& process);

		static CgiResult execute(const HttpRequest& request, const ServerConfig& server, const Location* location, const std::string& scriptPath, const std::string& interpreter);

	private:
		static std::string getScriptName(const std::string& scriptPath);
		static std::string getDirectoryPath(const std::string& path);
		static char** buildCgiEnv(const HttpRequest& request, const ServerConfig& server, const Location* location, const std::string& scriptPath);
		static void freeCgiEnv(char** envp);
		static std::string trim(const std::string& s);
		static int parseCgiStatusCode(const std::string& value);
		static HttpResponse buildResponseFromCgiOutput(const std::string& output);
};

#endif