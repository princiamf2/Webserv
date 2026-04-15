#ifndef CGITYPES_HPP
# define CGITYPES_HPP

# include <string>
# include "HttpResponse.hpp"

struct CgiResult
{
	bool		success;
	HttpResponse	response;
	std::string	rawOutput;

	CgiResult() : success(false) {}
};

#endif