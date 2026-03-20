
#ifndef PARSECONFIG_HPP
#define PARSECONFIG_HPP

#include <fstream>
#include <vector>
#include "ServerConfig.hpp"

std::vector<ServerConfig> parseConfig(std::string path);

#endif