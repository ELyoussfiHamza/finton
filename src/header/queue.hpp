#pragma once
#include "request.hpp" 
#include <queue>
#include <optional>
#include <mutex>
#include <condition_variable>

class Queue{
private:
    int MaxSize;
    std::queue<Request> RequestQueue;
    
    std::mutex mtx;
    std::condition_variable cv;
    bool IStopping = false;

public:
    Queue(int MaxSize ):MaxSize(MaxSize){};

    bool EnqueRequest(Request&& req);
    std::optional<Request> NextRequest(const std::chrono::steady_clock::time_point& TimePoint);
    std::optional<Request> NextRequestBlocking();
    
    void SetMaxSize(int size);
    void SetIStopping(bool New);
    bool GetIStopping ();

    void Shutdown();
};