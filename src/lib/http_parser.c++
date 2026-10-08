#include "http_parser.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

static std::string ToLower(std::string text){
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return text;
}

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

static std::size_t BodyLength(const HttpRequest& request,size_t cap){
    auto value = request.Header("content-length");
    if (!value){
        return 0;
    }
    bool digits = !value->empty() && value->size() <= 9 &&
                  std::all_of(value->begin(), value->end(),
                              [](unsigned char c){ return std::isdigit(c); });
    if (!digits){
        throw HttpParseError("invalid Content-Length");
    }
    auto Cl = std::stoul(*value);
    if (Cl > cap){
        throw HttpBodyMax("The body is tooo laaarrge ,  dropping this request");
    }
    return Cl;
}

HttpRequest ParseHttpRequest(const std::string& raw , size_t header_cap , size_t body_cap){
    const std::string LineEnd = "\r\n";

    auto head_end = raw.find("\r\n\r\n");
    if (head_end == std::string::npos){

        if (raw.size()> header_cap ){
            throw HttpHeaderMax("The header is too big , dropping request");
        }
        throw HttpIncompleteError("the blank line that ends the headers has not arrived");
    }else if (head_end >header_cap ){
        throw HttpHeaderMax("The header received is toooo long");
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

    std::size_t length = BodyLength(request,body_cap);
    std::size_t body_start = head_end + 4;
    if (raw.size() - body_start < length){
        throw HttpIncompleteError("the body is shorter than Content-Length");
    }
    request.body = raw.substr(body_start, length);
    return request;
}
