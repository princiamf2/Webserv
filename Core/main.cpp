#include "webserv.hpp"


int main(int ac, char **av)
{
	if (ac != 2)
	{
		std::cerr << "Usage: " << av[0] << " <config>" << std::endl;
		return (1);
	}

	std::vector<ServerConfig> configs = parseConfig(av[1]);
	Core C(configs);
	C.debug();
	C.runPoll();

	// loop(servers); //will need to fork to handle multiple servers
	return (0);
}
