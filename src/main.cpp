#include "scheduler.hpp"
#include "workers.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

int main(){
    const int NumWorkers = 2;
    const int NumClients = 3;
    const int RequestsPerClient = 4;

    // The scheduler is declared before the workers : it must outlive the
    // threads that hold a reference to it (destruction is in reverse order).
    Scheduler scheduler(100ms, 4, 100);
    Workers workers(NumWorkers);

    workers.InitiateWorkers(
        [&workers](Scheduler& sch, int thread_id){ workers.Task(sch, thread_id); },
        scheduler);

    // Clients stand in for the HTTP threads : each one submits its requests
    // with a small pause, all clients at the same time.
    std::atomic<int> Accepted = 0;
    std::vector<std::thread> Clients;
    for (int c = 0; c < NumClients; c++){
        Clients.emplace_back([&scheduler, &Accepted, c, RequestsPerClient]{
            for (int j = 0; j < RequestsPerClient; j++){
                // id = client * 100 + sequence, so the output shows who sent what
                if (scheduler.SubmitRequest(Request(c * 100 + j, 0))){
                    Accepted++;
                }
                std::this_thread::sleep_for(30ms);
            }
        });
    }
    for (auto& client : Clients){
        client.join();
    }
    std::cout << "Clients done : " << Accepted << "/"
              << NumClients * RequestsPerClient << " requests accepted" << std::endl;

    // There is no "request finished" signal yet (that is the promise/future of
    // milestone 4), so just leave the workers enough time to finish. The
    // shutdown is an abort : anything still queued at that point is dropped.
    std::this_thread::sleep_for(6s);

    std::cout << "Shutting down" << std::endl;
    scheduler.ForwardShutDown();

    bool AcceptedAfterShutdown = scheduler.SubmitRequest(Request(999, 0));
    std::cout << "Submit after shutdown accepted : " << std::boolalpha
              << AcceptedAfterShutdown << std::endl;

    // ~Workers joins the threads here.
    return 0;
}
