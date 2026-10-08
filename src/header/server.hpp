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
    int Backlog = 128;                  // connections the kernel holds before accept
    std::size_t HeaderCap = 8000;       // 8KB
    std::size_t BodyCap = 10000000;     // ~ 10 MB
    // One thread serves one connection, and it sleeps while its request is in
    // the scheduler. So this is also the most requests that can be in flight :
    // keep it above MaxBatchSize * workers, or the batches can never fill.
    int HttpThreads = 64;
    int MaxPendingConnections = 64;     // accepted, waiting for a free thread. Beyond : 503
    int IdleTimeoutSeconds = 5;         // a silent client is dropped after this
};

class Server{
private:
    ServerConfig Config;
    Scheduler& MainScheduler;
    // Several connection threads take ids at the same time
    std::atomic<int> NextRequestId{0};

    // Becomes readable when Stop() is called. It is never read, so it stays
    // readable and wakes every thread that polls it.
    int StopFd;
    std::atomic<bool> Stopping{false};

    // Accepted connections waiting for a thread
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
    // Owns client_fd from the first read to the close
    void HandleConnection(int client_fd);
    int ReadRequest(int client_fd, std::string& raw, HttpRequest& request);
    Answer Respond(const HttpRequest& http);
public:
    Server(const ServerConfig& config , Scheduler& scheduler);
    ~Server();
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    // Blocks until Stop() is called, then finishes the requests already
    // received and returns 0. Returns 1 when the server could not start.
    int StartServer();
    // Safe to call from a signal handler
    void Stop();
};
