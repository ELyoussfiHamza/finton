#pragma once
#include "http_parser.hpp"
#include "scheduler.hpp"
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

struct ServerConfig{
    int Port = 3001;
    int Backlog = 128;
    std::size_t HeaderCap = 8000;
    std::size_t BodyCap = 10000000;
    int HttpThreads = 64;
    int MaxPendingConnections = 64;
    int IdleTimeoutSeconds = 5;
};

class Server{
private:
    ServerConfig Config;
    Scheduler& MainScheduler;
    std::atomic<int> NextRequestId{0};

    int StopFd;
    std::atomic<bool> Stopping{false};

    std::queue<int> Pending;
    std::mutex PendingMutex;
    std::condition_variable PendingCv;
    bool Draining = false;
    std::vector<std::thread> HttpThreads;

    struct Answer{
        int status;
        std::string body;
    };

    void ConnectionLoop();
    void HandleConnection(int client_fd);
    int ReadRequest(int client_fd, std::string& raw, HttpRequest& request);
    Answer Respond(const HttpRequest& http);
public:
    Server(const ServerConfig& config , Scheduler& scheduler);
    ~Server();
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    int StartServer();
    void Stop();
};
