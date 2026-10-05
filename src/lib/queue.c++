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
    // For now we won't wait (CPU expensive) to fix later with cv
    auto& _q = RequestQueue; 
    
    auto Success =  cv.wait_until(lock ,TimePoint, [&_q] { return _q.size() > 0;});
    
    if (!Success){
        return std::nullopt;
    }

    Request Next = std::move(RequestQueue.front());
    RequestQueue.pop();
    return Next;
}

std::optional<Request> Queue::NextRequestBlocking(){
    std::unique_lock<std::mutex> lock(mtx);
   
    auto& _q = RequestQueue; 
    
    cv.wait(lock , [&_q] { return _q.size() > 0;});
    
    Request Next = std::move(RequestQueue.front());
    RequestQueue.pop();
    return Next;
}

bool Queue::EnqueRequest(Request&& req){
    std::unique_lock<std::mutex> lock(mtx);
    int _size = RequestQueue.size();
    if (_size >= MaxSize){
        lock.unlock();
        std::cout << "The queue is full; request rejected." << std::endl;
        return false;
    }
    RequestQueue.push(std::move(req));
    cv.notify_one();
    return true;
}