#include "webserv.hpp"


int main(int ac, char **av)
{
	if (ac != 2)
	{
		std::cerr << "Usage: " << av[0] << " <config>" << std::endl;
		return (1);
	}

	std::vector<ServerConfig> configs = parseConfig(av[1]);
	std::vector<Server> servers;

	for (size_t i = 0; i < configs.size(); i++)
	{
		Server s(configs[i]);
		if (s.start() != SUCCESS)
			return (FAIL);
		servers.push_back(s);
	}

	// loop(servers); //will need to fork to handle multiple servers
	return (servers[0].loop());
}
