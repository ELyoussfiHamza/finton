#pragma once

class Server{
private:
    int Port;
    int WaitOnQueue;
public:
    Server(int port , int WaitOnLoad): Port(port) , WaitOnQueue(WaitOnLoad){};
    int StartServer();
};
