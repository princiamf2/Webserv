#include "HttpRequest.hpp"

HttpRequest::HttpRequest() : isMultipart(false), isChunked(false) {}
HttpRequest::HttpRequest(HttpRequest const& other)
    : method(other.method),
      uri(other.uri),
      path(other.path),
      query(other.query),
      version(other.version),
      headers(other.headers),
      body(other.body),
      isMultipart(other.isMultipart),
      isChunked(other.isChunked),
      uploadFilename(other.uploadFilename),
      uploadContentType(other.uploadContentType),
      uploadContent(other.uploadContent) {}

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
        isMultipart = other.isMultipart;
        isChunked = other.isChunked;
        uploadFilename = other.uploadFilename;
        uploadContentType = other.uploadContentType;
        uploadContent = other.uploadContent;
    }
    return *this;
}

HttpRequest::~HttpRequest() {}
