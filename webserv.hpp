#pragma once

//====================(INCLUDES)============================//
#include <iostream>
#include <sys/socket.h>
#include <poll.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "colors.hpp"


//====================(DEFINES)=============================//
# define SUCCESS 1
# define FAIL 0

//====================(STRUCTS)=============================//


//====================(DECLARATIONS)========================//


//============(PRINCIPAL)================//
//main loop
int loop();

//============(UTILS)====================//

//clear

//debug

