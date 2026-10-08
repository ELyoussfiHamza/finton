#pragma once
#include <map>
#include <optional>
#include <stdexcept>
#include <string>

class HttpParseError : public std::runtime_error{
public:
    using std::runtime_error::runtime_error;
};


class HttpIncompleteError : public HttpParseError{
public:
    using HttpParseError::HttpParseError;
};

class HttpHeaderMax : public HttpParseError{
public:
    using HttpParseError::HttpParseError;
};

class HttpBodyMax : public HttpParseError{
public:
    using HttpParseError::HttpParseError;
};
struct HttpRequest{
    std::string method;   
    std::string path;     
    std::string version;  

    std::map<std::string, std::string> headers;
    std::string body;

    std::optional<std::string> Header(const std::string& name) const;
};

HttpRequest ParseHttpRequest(const std::string& raw, size_t header_cap , size_t body_cap);
