#include <iostream>
#include "queue.hpp"
#include <optional> 
#include "request.hpp"
#include <mutex>




void Queue::SetMaxSize(int _MaxSize){
    std::unique_lock<std::mutex> lock(mtx);  
    MaxSize = _MaxSize;
}

std::optional<Request> Queue::NextRequest(const std::chrono::steady_clock::time_point& TimePoint){
    std::unique_lock<std::mutex> lock(mtx);
    auto& _q = RequestQueue;
    auto& Istopping = IStopping;
    auto Success =  cv.wait_until(lock ,TimePoint, [&_q,&Istopping] { return _q.size() > 0 || Istopping;});

    // Draining : while stopping we keep handing out what is still queued
    if (!Success or (Istopping and _q.size() == 0) ){
        return std::nullopt;
    }

    Request Next = std::move(RequestQueue.front());
    RequestQueue.pop();
    return Next;
}

std::optional<Request> Queue::NextRequestBlocking(){
    std::unique_lock<std::mutex> lock(mtx);
   
    auto& _q = RequestQueue; 
    auto& Istopping = IStopping;

    cv.wait(lock , [&_q , &Istopping] { return  _q.size() > 0 || Istopping ;});
    
    if (Istopping && _q.size() == 0){
        return std::nullopt;
    }
    Request Next = std::move(RequestQueue.front());
    RequestQueue.pop();
    return Next;
}

bool Queue::EnqueRequest(Request&& req){
    std::unique_lock<std::mutex> lock(mtx);
    if (IStopping){
        return false;
    }
    int _size = RequestQueue.size();
    if (_size >= MaxSize){
        return false;
    }
    RequestQueue.push(std::move(req));
    cv.notify_one();
    return true;
}

void Queue::Shutdown(){
    std::unique_lock<std::mutex> lock(mtx);  
    IStopping = true; 
    cv.notify_all();
}
