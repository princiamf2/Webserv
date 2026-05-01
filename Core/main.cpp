#include "webserv.hpp"
#include <signal.h>

void	sa(int sig)
{
	(void)sig;
	std::cout << std::endl << "To quit, just type 'q' and enter" << std::endl;
}

int main(int ac, char **av)
{
	if (ac != 2)
	{
		std::cerr << "Usage: " << av[0] << " <config>" << std::endl;
		return (1);
	}
	signal(SIGINT, sa);
	signal(SIGQUIT, sa);
	signal(SIGPIPE, SIG_IGN);
	try
	{
		std::vector<ServerConfig> configs = parseConfig(av[1]);
		if (configs.empty())
			return (1);
		Core C(configs);
		welcome();
		C.runPoll();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return (1);
	}

	return (0);
}
