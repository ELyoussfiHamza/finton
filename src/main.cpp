#include "server.hpp"


int main(){
    int port = 3001;
    int Backlogs = 5;
    auto srv = Server(port ,Backlogs);

    srv.StartServer();
}