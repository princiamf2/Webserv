#include "HttpResponse.hpp"

HttpResponse::HttpResponse()
	: statusCode(200), reasonPhrase("OK")
{
}

HttpResponse::HttpResponse(HttpResponse const& other)
	: statusCode(other.statusCode),
	  reasonPhrase(other.reasonPhrase),
	  headers(other.headers),
	  body(other.body)
{
}

HttpResponse& HttpResponse::operator=(HttpResponse const& other)
{
	if (this != &other)
	{
		statusCode = other.statusCode;
		reasonPhrase = other.reasonPhrase;
		headers = other.headers;
		body = other.body;
	}
	return *this;
}

HttpResponse::~HttpResponse() {}
