
#ifndef PARSECONFIG_HPP
#define PARSECONFIG_HPP

#include <fstream>
#include <sstream>
#include <vector>
#include "ServerConfig.hpp"

std::string stripSemicolon(const std::string& s);
std::vector<ServerConfig> parseConfig(std::string path);
bool parseServer(std::istringstream& stream, ServerConfig& server);
bool parseLocation(std::istringstream& lineStream, std::istringstream& stream, ServerConfig& server);

bool expectOpenBracket(std::istringstream& current_line, std::istringstream& stream);
bool parseServer(std::istringstream& stream, ServerConfig& server);
bool parseLocation(std::istringstream& lineStream, std::istringstream& stream, ServerConfig& server);

#endif