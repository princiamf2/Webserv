#ifndef CGIPROCESS_HPP
# define CGIPROCESS_HPP

#include <ctime>
# include <string>
# include <sys/types.h>

struct CgiProcess
{
	pid_t		pid;
	int			stdinFd;
	int			stdoutFd;
	std::string	inputBuffer;
	size_t		inputOffset;
	std::string	outputBuffer;
	bool		stdinClosed;
	bool		stdoutClosed;
	bool		childExited;
	int			exitStatus;
	bool		error;
	time_t		startTime;

	CgiProcess()
		: pid(-1), stdinFd(-1), stdoutFd(-1), inputOffset(0),
		  stdinClosed(false), stdoutClosed(false),
		  childExited(false), exitStatus(0), error(false), startTime(0) {}
};

#endif