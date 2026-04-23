#include "HttpRequest.hpp"

HttpRequest::HttpRequest() {}
HttpRequest::HttpRequest(HttpRequest const& other)
    : method(other.method),
      uri(other.uri),
      path(other.path),
      query(other.query),
      version(other.version),
      headers(other.headers),
      body(other.body) {}

HttpRequest& HttpRequest::operator=(HttpRequest const& other)
{
    if (this != &other)
    {
        method = other.method;
        uri = other.uri;
        path = other.path;
        query = other.query;
        version = other.version;
        headers = other.headers;
        body = other.body;
    }
    return *this;
}

HttpRequest::~HttpRequest() {}
