#pragma once
#include <map>
#include <optional>
#include <stdexcept>
#include <string>

// The text is not valid HTTP : answer 400.
class HttpParseError : public std::runtime_error{
public:
    using std::runtime_error::runtime_error;
};

// The text is valid so far but the request is not all there yet (TCP can
// deliver it in pieces) : read more bytes and parse again.
class HttpIncompleteError : public HttpParseError{
public:
    using HttpParseError::HttpParseError;
};

struct HttpRequest{
    std::string method;   // "GET", "POST", ... exactly as sent
    std::string path;     // "/infer"
    std::string version;  // "HTTP/1.1"
    // Header names are stored in lowercase, because HTTP header names are
    // case-insensitive. Values are kept as sent, without surrounding spaces.
    std::map<std::string, std::string> headers;
    std::string body;

    // Looks a header up by its lowercase name. Empty when it was not sent.
    std::optional<std::string> Header(const std::string& name) const;
};

// Parses one complete HTTP request. It only checks the syntax : whether a
// header is required or a method is allowed is for the caller to decide.
HttpRequest ParseHttpRequest(const std::string& raw);
