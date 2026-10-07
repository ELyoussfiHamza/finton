#include "server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <stdexcept>

std::ostream& operator<<(std::ostream& os, ContentType s) {
    switch (s) {
        case ContentType::JSON: return os << "JSON";
        case ContentType::OCTETSTREAM: return os << "OCTETSTREAM";
    }
    return os << "Unsupported";
}
int Server::StartServer(){

    // Create file descriptor (a socket ): a way to communicate 
    // We use TCP / ipv4
    int ServerSocket = socket(AF_INET , SOCK_STREAM , 0);

    if (ServerSocket == -1){
        std::cerr << std::strerror(errno) << std::endl;
        return 1;
    }
    // In case server closed and we reconnect we can reuse the port 
    // we set  the option to 1 which means true we want that feature:port-reuse
    int opt = 1;
    setsockopt(ServerSocket , SOL_SOCKET , SO_REUSEADDR,&opt,sizeof(opt));

    //Hna: Here we setup server address 
    struct sockaddr_in server_address {};
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    // htons is used to do the proper bytes formatting
    server_address.sin_port = htons(Port);

    if (bind(ServerSocket , (struct sockaddr*)& server_address , sizeof(server_address)) == -1){
        std::cerr << std::strerror(errno) << std::endl;
        return 1;
    }

    //Listen for connections
    if (listen(ServerSocket , WaitOnQueue) < 0){
        std::cerr << std::strerror(errno) << std::endl;
        return 1;
    }

    std::cout << "Your wonderful hand written lol server is listening on " << Port << std::endl;
    
    while (true){
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_fd = accept(ServerSocket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0) {
              std::cerr << std::strerror(errno) << std::endl;
            continue;
        }

        // Read request
        char buffer[1024] = {0};
        read(client_fd, buffer, sizeof(buffer) - 1);
        std::string request(buffer);
        auto ct = GetContentType(request);
        auto cl = GetContentLength(request);

        json data = GetJson(request);
        std::cout << data << std::endl;
    }
}

ContentType Server::GetContentType(std::string& request){
    std::string _Ct = "Content-Type: ";
    int idx = request.find(_Ct)+_Ct.size();
    int start = idx;
    while (request.at(idx) != '\r'){
        idx++;
    }
    auto Ct = request.substr(start,idx-start);
    if (Ct == "application/json"){
        return ContentType::JSON;
    }else {
        throw std::runtime_error("Unsupported Content-Type");
    }
}

int Server::GetContentLength(std::string& request){
        std::string _Cl = "Content-Length: ";
    int idx = request.find(_Cl)+_Cl.size();
    int start = idx;
    while (request.at(idx) != '\r'){
        idx++;
    }
    auto Cl = std::stoi(request.substr(start,idx-start));
    return Cl;
}

Method Server::GetMethod(std::string& request){
        int idx = 0;
        while (request.at(idx) != ' '){
            idx++;
        };
        auto string_method = request.substr(0 , idx);
        if ( string_method == "POST"){
            return Method::POST;
        }else if (request.substr(0,idx) == "GET"){
            return Method::GET;
        }else{
            throw std::runtime_error("Unsupported http Method");
        }
}

json Server::GetJson(std::string& request){
    int FirstCurlyIdx = request.find("{");
    int idx = FirstCurlyIdx;

    while (idx < request.size() and request.at(idx)!='\r'){
        idx++;
    }
    json data = json::parse(request.substr(FirstCurlyIdx,idx));
    return data;
}