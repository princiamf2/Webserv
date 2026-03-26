#include "./webserv.hpp"

int error(std::string s)
{
	std::cerr << ERROR << s << std::endl;
	perror(s.c_str());
	return (FAIL);
}
