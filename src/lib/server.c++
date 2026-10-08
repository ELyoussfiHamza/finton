#include "server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <stdexcept>

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
        std::cout << request << std::endl;
        close(client_fd);
    }
}
