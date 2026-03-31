#ifndef REQUESTUTILS_HPP
# define REQUESTUTILS_HPP

# include <string>
# include "HttpResponse.hpp"
# include "../Parsing/ServerConfig.hpp"
# include "../Parsing/Location.hpp"

std::string resolveRoot(ServerConfig const& server, Location const* location);
bool buildFilePath(std::string const& root, std::string const& path, Location const* location, ServerConfig const& server, std::string& filePath);
std::string getFileExtension(std::string const& filePath);
HttpResponse buildErrorResponse(ServerConfig const& server, int code, std::string const& path = "", bool directoryListingDenied = false);

#endif