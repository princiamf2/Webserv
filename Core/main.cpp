#include "webserv.hpp"

int main(int ac, char**av)
{
	if (ac != 2)
	{
		std::cerr << ERROR << "Usage: " << av[0] << " <path-to-config-file>" << std::endl;
		return (1);
	}
	std::vector<ServerConfig> servers = parseConfig(av[1]);
	int err = FAIL;
	if ((err = loop(servers)) != SUCCESS) // with struct as arg
		return (err);
}	
