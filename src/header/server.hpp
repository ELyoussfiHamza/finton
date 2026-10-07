#pragma once 
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
enum class ContentType{
    // Will be handling these two for now 
    JSON,
    OCTETSTREAM
};
enum class Method{
    // will be handling these two for now
    POST,
    GET
};

class Server{
private:
    int Port;
    int WaitOnQueue;
public:
    Server(int port , int WaitOnLoad): Port(port) , WaitOnQueue(WaitOnLoad){};
    int StartServer();

    ContentType GetContentType (std::string& request);
    int GetContentLength(std::string& request);
    Method GetMethod(std::string& request);

    json GetJson(std::string& body);
};