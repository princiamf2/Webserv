#include "./webserv.hpp"
#include <sstream>

int error(std::string s)
{
	std::cerr << ERROR << s << std::endl;
	perror(s.c_str());
	return (FAIL);
}

std::string toString(int n)
{
	std::ostringstream oss;
	oss << n;
	return (oss.str());
}

void logs(std::string s)
{
	std::cout << "LOGS --->    " << s << std::endl;
}

void welcome()
{
	std::cout << "======================================" << std::endl;
	std::cout << "|     !!! Welcome to Webserv !!!     |" << std::endl;
	std::cout << "|                                    |" << std::endl;
	std::cout << "| Type h to show the help page       |" << std::endl;
	std::cout << "======================================" << std::endl;
}

void help()
{
	std::cout << "======================================" << std::endl;
	std::cout << "| Help page:                         |" << std::endl;
	std::cout << "|                                    |" << std::endl;
	std::cout << "| Type h to show this page           |" << std::endl;
	std::cout << "|                                    |" << std::endl;
	std::cout << "| h: show help page                  |" << std::endl;
	std::cout << "| q: quit Webserv                    |" << std::endl;
	std::cout << "|                                    |" << std::endl;
	std::cout << "| l: show logs                       |" << std::endl;
	std::cout << "|   - l<index>: show only a          |" << std::endl;
	std::cout << "|               specific server      |" << std::endl;
	std::cout << "======================================" << std::endl;
}

int readCommand(Core* C)
{
    char line[4];
    ssize_t size = read(0, line, 3);

	if (size < 0)
		return (0);
	if (size == 0)
		return (QUIT);

    if (size != 2 && size != 3)
        return (0);

    line[size] = '\0';

    if (line[0] == 'q')
    {
		std::cout << "Quitting...\n" << std::endl;
        return (QUIT);
    }
	else if (line[0] == 'h')
		help();
    else if (line[0] == 'l')
    {
		int index = line[1] - '0';
		if (index >= 0 && index < (int)C->getServers().size())
		{
			std::cout << std::endl << "[Server " << index << "] @ " << &C->getServers()[index] << std::endl;
			C->getServers()[index].debug();
		}
		else
			C->debug();
    }
	return (0);
}
