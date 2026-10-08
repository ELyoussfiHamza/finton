#include "http_parser.hpp"
#include <iostream>
#include <string>

static int Failures = 0;

static const std::size_t HeaderCap = 8000;
static const std::size_t BodyCap = 10000000;

static HttpRequest Parse(const std::string& raw){
    return ParseHttpRequest(raw, HeaderCap, BodyCap);
}

static void Check(bool condition, const std::string& what){
    if (!condition){
        std::cout << "FAIL : " << what << std::endl;
        Failures++;
    }
}

template <typename E>
static bool Throws(const std::string& raw){
    try{
        Parse(raw);
    }catch (const E&){
        return true;
    }catch (...){
    }
    return false;
}

static bool Malformed(const std::string& raw){
    return Throws<HttpParseError>(raw) && !Throws<HttpIncompleteError>(raw);
}

int main(){
    {
        auto r = Parse(
            "GET /health HTTP/1.1\r\nHost: localhost:3001\r\nAccept: */*\r\n\r\n");
        Check(r.method == "GET", "GET : method");
        Check(r.path == "/health", "GET : path");
        Check(r.version == "HTTP/1.1", "GET : version");
        Check(r.Header("host") == "localhost:3001", "GET : host");
        Check(!r.Header("content-length"), "GET : no content-length");
        Check(r.body.empty(), "GET : empty body");
    }
    {
        std::string body = R"({"id": 1, "input": [1.0, 2.5, 3.0]})";
        auto r = Parse(
            "POST /infer HTTP/1.1\r\nHost: localhost:3001\r\nUser-Agent: curl/7.81.0\r\n"
            "Accept: */*\r\nContent-Type: application/json\r\nContent-Length: " +
            std::to_string(body.size()) + "\r\n\r\n" + body);
        Check(r.method == "POST", "POST : method");
        Check(r.Header("content-type") == "application/json", "POST : content-type");
        Check(r.body == body, "POST : body");
    }
    {
        auto r = Parse(
            "POST /infer HTTP/1.1\r\ncontent-length:2\r\n"
            "User-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\nCONTENT-TYPE:  application/json  \r\n\r\nok");
        Check(r.body == "ok", "order : body");
        Check(r.Header("content-type") == "application/json", "order : value trimmed, name lowercased");
        Check(r.Header("user-agent") == "Mozilla/5.0 (X11; Linux x86_64)", "order : value with spaces");
    }
    {
        auto r = Parse("POST /infer HTTP/1.1\r\nContent-Length: 2\r\n\r\nokEXTRA");
        Check(r.body == "ok", "extra bytes : body stops at Content-Length");
    }
    {
        auto r = Parse("GET / HTTP/1.1\r\n\r\n");
        Check(r.path == "/" && r.headers.empty(), "no headers");
    }

    Check(Throws<HttpIncompleteError>("POST /infer HTTP/1.1\r\nHost: x\r\n"), "incomplete : no blank line");
    Check(Throws<HttpIncompleteError>("POST /infer HTTP/1.1\r\nContent-Length: 10\r\n\r\nabc"), "incomplete : short body");
    Check(Throws<HttpIncompleteError>(""), "incomplete : empty input");

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

    std::string long_header = "GET / HTTP/1.1\r\nX-Pad: " + std::string(HeaderCap, 'a');
    Check(Throws<HttpHeaderMax>(long_header), "too large : head over the cap, no blank line yet");
    Check(Throws<HttpHeaderMax>(long_header + "\r\n\r\n"), "too large : head over the cap, arrived whole");
    Check(Throws<HttpBodyMax>("POST / HTTP/1.1\r\nContent-Length: " + std::to_string(BodyCap + 1) + "\r\n\r\n"),
          "too large : Content-Length over the cap");

    std::string big_body(HeaderCap * 2, 'b');
    std::string big_request = "POST /infer HTTP/1.1\r\nContent-Length: " +
                              std::to_string(big_body.size()) + "\r\n\r\n" + big_body;
    Check(!Throws<HttpParseError>(big_request), "body larger than the header cap is accepted");
    Check(Throws<HttpIncompleteError>(big_request.substr(0, HeaderCap + 100)),
          "body larger than the header cap, partly arrived : incomplete");

    std::cout << (Failures == 0 ? "ALL PASSED" : "FAILED") << std::endl;
    return Failures == 0 ? 0 : 1;
}
