// HTTPRequest/HttpModule.hpp
#ifndef HTTPMODULE_HPP
#define HTTPMODULE_HPP

#include <string>
#include "../Parsing/ServerConfig.hpp"

std::string handleRawHttpRequest(std::string const& rawRequest,
                                 ServerConfig const& server);

#endif