#include "scheduler.hpp"
#include <algorithm>
#include <mutex>



Scheduler::Scheduler(std::chrono::milliseconds md , int mbs ,int mqs) : MaxDelay(md) , MaxBatchSize(std::max(1,mbs)),MainQueue(mqs){};



std::chrono::milliseconds Scheduler::GetMaxDelay(){
    return MaxDelay;
}

int Scheduler::GetMaxBatchSize(){
    return MaxBatchSize;
}

std::optional<std::vector<Request>> Scheduler::PullRequests(){
    // Shutdown is a drain : batches keep coming until the queue is empty
    std::unique_lock<std::mutex> lock(m);
    std::vector<Request> Batch;
    Batch.reserve(MaxBatchSize);
    auto FirstRequest = MainQueue.NextRequestBlocking();
    if (FirstRequest == std::nullopt){
        // Stopping and nothing left to drain
        return std::nullopt;
    }
    Batch.push_back(std::move(*FirstRequest));

    // The delay counts from the arrival of the oldest request of the batch
    auto EndPoint = Batch[0].GetArrival() + MaxDelay;
    while (true){
        if (Batch.size() >= MaxBatchSize) break;
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