#include "webserv.hpp"
#include <signal.h>

void	sa(int sig)
{
	(void)sig;
	std::cout << std::endl << "To quit, just type 'q' and enter" << std::endl;
}

int main(int ac, char **av)
{
	if (ac > 2)
	{
		std::cerr << ERROR << "Usage: " << av[0] << " [config]" << std::endl;
		return (1);
	}

	std::string conf;
	if (ac == 1)
	{
		std::cout << WARNING << " no config file given, using the default one (./configs/Core.config)" << std::endl;
		conf = "./configs/Core.config";
	}
	else
		conf = av[1];

	signal(SIGINT, sa);
	signal(SIGQUIT, sa);
	signal(SIGPIPE, SIG_IGN);
	try
	{
		std::vector<ServerConfig> configs = parseConfig(conf);
		if (configs.empty())
		{
			std::cerr << ERROR << "No server detected in the config file..." << std::endl;
			return (1);
		}
		Core C(configs);
		welcome();
		C.runPoll();
	}
	catch (const std::exception &e)
	{
		std::cerr << ERROR << e.what() << std::endl;
		return (1);
	}

	return (0);
}
