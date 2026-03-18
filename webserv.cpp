#include "webserv.hpp"

int error(std::string s)
{
	std::cerr << ERROR << s << std::endl;
	return (FAIL);
}

#include <cstring>
int loop()
{
	while (true) //to change
	{
		// AF_INET: IPV4, SOCK_STREAM: TCP
		int fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd == -1)
			return (error("Socket returned -1"));
		int opt = 1;
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

		int flags = fcntl(fd, F_GETFL, 0);
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(8080); //to change
		addr.sin_addr.s_addr = INADDR_ANY; // touttes les interfaces
		if (bind (fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
			return (error("Bind returned -1"));
		if (listen(fd, SOMAXCONN) == -1)
			return (error("Listen returned -1"));

		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);
		int client_fd = accept(fd, (struct sockaddr*)&client_addr, &client_len); // give a new fd associated to the client so the listen can continue
		(void)client_fd;
		error("aaaaaaaaa");

	}
	return (SUCCESS);
}
