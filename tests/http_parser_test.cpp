#include "http_parser.hpp"
#include <iostream>
#include <string>

// Tests for the HTTP parser. No socket involved : each case is just a string.

static int Failures = 0;

static void Check(bool condition, const std::string& what){
    if (!condition){
        std::cout << "FAIL : " << what << std::endl;
        Failures++;
    }
}

// True when parsing `raw` throws exactly the error type E.
template <typename E>
static bool Throws(const std::string& raw){
    try{
        ParseHttpRequest(raw);
    }catch (const E&){
        return true;
    }catch (...){
    }
    return false;
}

// HttpIncompleteError is also an HttpParseError, so "malformed" has to mean
// a parse error that is not an incomplete one.
static bool Malformed(const std::string& raw){
    return Throws<HttpParseError>(raw) && !Throws<HttpIncompleteError>(raw);
}

int main(){
    {   // GET without a body
        auto r = ParseHttpRequest(
            "GET /health HTTP/1.1\r\nHost: localhost:3001\r\nAccept: */*\r\n\r\n");
        Check(r.method == "GET", "GET : method");
        Check(r.path == "/health", "GET : path");
        Check(r.version == "HTTP/1.1", "GET : version");
        Check(r.Header("host") == "localhost:3001", "GET : host");
        Check(!r.Header("content-length"), "GET : no content-length");
        Check(r.body.empty(), "GET : empty body");
    }
    {   // what curl sends for a JSON POST
        std::string body = R"({"id": 1, "input": [1.0, 2.5, 3.0]})";
        auto r = ParseHttpRequest(
            "POST /infer HTTP/1.1\r\nHost: localhost:3001\r\nUser-Agent: curl/7.81.0\r\n"
            "Accept: */*\r\nContent-Type: application/json\r\nContent-Length: " +
            std::to_string(body.size()) + "\r\n\r\n" + body);
        Check(r.method == "POST", "POST : method");
        Check(r.Header("content-type") == "application/json", "POST : content-type");
        Check(r.body == body, "POST : body");
    }
    {   // other order, lowercase names, no space after the colon, spaces in a value
        auto r = ParseHttpRequest(
            "POST /infer HTTP/1.1\r\ncontent-length:2\r\n"
            "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\nCONTENT-TYPE:  application/json  \r\n\r\nok");
        Check(r.body == "ok", "order : body");
        Check(r.Header("content-type") == "application/json", "order : value trimmed, name lowercased");
        Check(r.Header("user-agent") == "Mozilla/5.0 (X11; Linux x86_64)", "order : value with spaces");
    }
    {   // bytes after the body are not part of this request
        auto r = ParseHttpRequest("POST /infer HTTP/1.1\r\nContent-Length: 2\r\n\r\nokEXTRA");
        Check(r.body == "ok", "extra bytes : body stops at Content-Length");
    }
    {   // no headers at all
        auto r = ParseHttpRequest("GET / HTTP/1.1\r\n\r\n");
        Check(r.path == "/" && r.headers.empty(), "no headers");
    }

    // The request has not fully arrived yet
    Check(Throws<HttpIncompleteError>("POST /infer HTTP/1.1\r\nHost: x\r\n"), "incomplete : no blank line");
    Check(Throws<HttpIncompleteError>("POST /infer HTTP/1.1\r\nContent-Length: 10\r\n\r\nabc"), "incomplete : short body");
    Check(Throws<HttpIncompleteError>(""), "incomplete : empty input");

    // The request is malformed
    Check(Malformed("GET /health\r\n\r\n"), "malformed : two words in request line");
    Check(Malformed("GET /health HTTP/1.1 extra\r\n\r\n"), "malformed : four words in request line");
    Check(Malformed("GET health HTTP/1.1\r\n\r\n"), "malformed : path without /");
    Check(Malformed("GET /health FTP/1.1\r\n\r\n"), "malformed : bad version");
    Check(Malformed("GET / HTTP/1.1\r\nNoColonHere\r\n\r\n"), "malformed : header without colon");
    Check(Malformed("GET / HTTP/1.1\r\n: value\r\n\r\n"), "malformed : empty header name");
    Check(Malformed("GET / HTTP/1.1\r\nBad Name: value\r\n\r\n"), "malformed : space in header name");
    Check(Malformed("POST / HTTP/1.1\r\nContent-Length: abc\r\n\r\n"), "malformed : non-numeric length");
    Check(Malformed("POST / HTTP/1.1\r\nContent-Length: -5\r\n\r\n"), "malformed : negative length");
    Check(Malformed("POST / HTTP/1.1\r\nContent-Length: 99999999999999999999\r\n\r\n"), "malformed : huge length");

    std::cout << (Failures == 0 ? "ALL PASSED" : "FAILED") << std::endl;
    return Failures == 0 ? 0 : 1;
}
