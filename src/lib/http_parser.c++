#include "http_parser.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

static std::string ToLower(std::string text){
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return text;
}

// Removes spaces and tabs at both ends.
static std::string Trim(const std::string& text){
    auto first = text.find_first_not_of(" \t");
    if (first == std::string::npos){
        return "";
    }
    auto last = text.find_last_not_of(" \t");
    return text.substr(first, last - first + 1);
}

std::optional<std::string> HttpRequest::Header(const std::string& name) const{
    auto it = headers.find(name);
    if (it == headers.end()){
        return std::nullopt;
    }
    return it->second;
}

// "POST /infer HTTP/1.1" : exactly three words.
static void ParseRequestLine(const std::string& line, HttpRequest& request){
    std::istringstream ss(line);
    std::string extra;
    if (!(ss >> request.method >> request.path >> request.version) || (ss >> extra)){
        throw HttpParseError("request line must be : METHOD PATH VERSION");
    }
    if (!request.path.starts_with('/')){
        throw HttpParseError("path must start with /");
    }
    if (!request.version.starts_with("HTTP/")){
        throw HttpParseError("version must start with HTTP/");
    }
}

// "Content-Type: application/json" : the same code handles every header,
// whatever its name and wherever it appears.
static void ParseHeaderLine(const std::string& line, HttpRequest& request){
    auto colon = line.find(':');
    if (colon == std::string::npos || colon == 0){
        throw HttpParseError("header line must be : Name: value");
    }
    std::string name = line.substr(0, colon);
    if (name.find_first_of(" \t") != std::string::npos){
        throw HttpParseError("header name must not contain spaces");
    }
    request.headers[ToLower(name)] = Trim(line.substr(colon + 1));
}

// A missing Content-Length means there is no body.
static std::size_t BodyLength(const HttpRequest& request){
    auto value = request.Header("content-length");
    if (!value){
        return 0;
    }
    // Digits only, and short enough that the conversion cannot overflow.
    bool digits = !value->empty() && value->size() <= 9 &&
                  std::all_of(value->begin(), value->end(),
                              [](unsigned char c){ return std::isdigit(c); });
    if (!digits){
        throw HttpParseError("invalid Content-Length");
    }
    return std::stoul(*value);
}

HttpRequest ParseHttpRequest(const std::string& raw){
    const std::string LineEnd = "\r\n";

    // A blank line separates the head (request line + headers) from the body.
    auto head_end = raw.find("\r\n\r\n");
    if (head_end == std::string::npos){
        throw HttpIncompleteError("the blank line that ends the headers has not arrived");
    }
    std::string head = raw.substr(0, head_end);

    HttpRequest request;
    std::size_t pos = 0;
    bool first_line = true;
    while (pos <= head.size()){
        auto end = head.find(LineEnd, pos);
        if (end == std::string::npos){
            end = head.size();
        }
        std::string line = head.substr(pos, end - pos);

        if (first_line){
            ParseRequestLine(line, request);
            first_line = false;
        }else{
            ParseHeaderLine(line, request);
        }
        pos = end + LineEnd.size();
    }

    std::size_t length = BodyLength(request);
    std::size_t body_start = head_end + 4;
    if (raw.size() - body_start < length){
        throw HttpIncompleteError("the body is shorter than Content-Length");
    }
    request.body = raw.substr(body_start, length);
    return request;
}
