#ifndef REQUESTACTION_HPP
#define REQUESTACTION_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "RequestHandler.hpp"

enum ActionRequestType
{
	ACTION_IMMEDIATE_RESPONSE,
	ACTION_START_CGI
};

struct ActionRequest
{
	ActionRequestType type;
	HttpRequest request;
	HttpResponse response;
	Location const* location;
	std::string scriptPath;
	std::string interpreter;

	ActionRequest() : type(ACTION_IMMEDIATE_RESPONSE), location(NULL) {}
};
#endif
