#pragma once

//====================(INCLUDES)============================//
#include "./Core.hpp"
#include "./Server.hpp"

//====================(DEFINES)=============================//
# define SUCCESS 1
# define FAIL 0

//====================(STRUCTS)=============================//

//====================(DECLARATIONS)========================//

class Core;
class Server;

//============(UTILS)====================//
//error
int error(std::string s);

// s logs
void logs(std::string s);
std::string toString(int n);

//clear

//debug
