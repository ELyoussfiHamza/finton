#include "scheduler.hpp"
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

// Pulls one batch, prints its ids and how long the pull took.
// Returns the batch size (0 if the scheduler returned nothing).
static std::size_t PullAndPrint(Scheduler& scheduler){
    auto Start = std::chrono::steady_clock::now();
    auto Batch = scheduler.PullRequests();
    auto ElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - Start).count();

    if (!Batch){
        std::cout << "  no batch after " << ElapsedMs << " ms" << std::endl;
        return 0;
    }

    std::cout << "  batch [";
    for (auto& request : *Batch){
        std::cout << " " << request.GetId();
    }
    std::cout << " ] after " << ElapsedMs << " ms" << std::endl;
    return Batch->size();
}

int main(){
    bool Ok = true;

    // Test 1 : batch size 3, 5 requests queued.
    // Expect [0 1 2] immediately, then [3 4] once the 100 ms delay expires.
    {
        std::cout << "Test 1 : batching" << std::endl;
        Scheduler scheduler(100ms, 3, 10);
        for (int i = 0; i < 5; i++){
            Ok = scheduler.SubmitRequest(Request(i, 0)) && Ok;
        }
        Ok = (PullAndPrint(scheduler) == 3) && Ok;
        Ok = (PullAndPrint(scheduler) == 2) && Ok;
        // No third pull : the queue is empty, so it would block forever.
    }

    // Test 2 : queue of size 4, the 5th submit must be rejected.
    {
        std::cout << "Test 2 : full queue" << std::endl;
        Scheduler scheduler(100ms, 3, 4);
        for (int i = 0; i < 4; i++){
            Ok = scheduler.SubmitRequest(Request(i, 0)) && Ok;
        }
        bool Accepted = scheduler.SubmitRequest(Request(4, 0));
        std::cout << "  5th submit accepted : " << std::boolalpha << Accepted << std::endl;
        Ok = !Accepted && Ok;
    }

    std::cout << (Ok ? "ALL PASSED" : "FAILED") << std::endl;
    return Ok ? 0 : 1;
}
