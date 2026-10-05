#pragma once
#include "queue.hpp"
#include <chrono>
#include <optional>
#include <vector>
#include <mutex>
//Prioritizing :
// latency / throughput 
// Latency is low <-> throughput is low 

class Scheduler{
private:
    std::chrono::milliseconds MaxDelay; // Wait for requests before submitting a batch
    int MaxBatchSize; // The maxSize of a batch : never surpassed
    Queue MainQueue;
    std::mutex mtx; // This mutex is not to protect the queue (has its own)
    // Its for ensuring one thread is constructing the batch at a time therefore
    // we avoid having two or more half filled batch
public:
    Scheduler(std::chrono::milliseconds md , int mbs , int mqs);
    
    void SetMaxDelay(std::chrono::milliseconds _maxdelay);
    void SetMaxBatchSize(int maxbatchsize);
    
    std::chrono::milliseconds GetMaxDelay();
    int GetMaxBatchSize();

    std::optional<std::vector<Request>> PullRequests();

    bool SubmitRequest(Request&& req);
};