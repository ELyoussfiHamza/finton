#include "scheduler.hpp"
#include <mutex>



Scheduler::Scheduler(std::chrono::milliseconds md , int mbs ,int mqs) : MaxDelay(md) , MaxBatchSize(mbs),MainQueue(mqs){};

void Scheduler::SetMaxBatchSize(int mbs){
    MaxBatchSize = mbs;
}

void Scheduler::SetMaxDelay( std::chrono::milliseconds md){
    MaxDelay = md;
}

std::chrono::milliseconds Scheduler::GetMaxDelay(){
    return MaxDelay;
}

int Scheduler::GetMaxBatchSize(){
    return MaxBatchSize;
}

std::optional<std::vector<Request>> Scheduler::PullRequests(){
    // wait for maxdely
    // By default is an abort shutdown 
    std::unique_lock<std::mutex> lock(m);
    std::vector<Request> Batch;
    Batch.reserve(MaxBatchSize);
    auto FirstRequest = MainQueue.NextRequestBlocking();    
    if (FirstRequest == std::nullopt){
        // Is stopping 
        return std::nullopt;
    }
    Batch.push_back(std::move(*FirstRequest));
    if (MainQueue.GetIStopping()){
        return Batch;
    }

    auto EndPoint = std::chrono::steady_clock::now() + MaxDelay;
    while (true){
        if (Batch.size() == MaxBatchSize) break;
        auto request = MainQueue.NextRequest(EndPoint);
        if (request != std::nullopt){
            Batch.push_back(std::move(*request));
        }else{
            // timeout  or shutdown
            break;
        }
    }
    return Batch;
}

bool Scheduler::SubmitRequest(Request&& req){
    bool _outcome = MainQueue.EnqueRequest(std::move(req));
    return _outcome;
}

void Scheduler::ForwardShutDown(){
    MainQueue.Shutdown();
}