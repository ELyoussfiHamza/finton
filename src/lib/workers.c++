#include "workers.hpp"
#include "scheduler.hpp"
#include <thread>
#include <chrono>

Workers::Workers(int max):MaxPoolWorkers(max){};

Workers::~Workers(){
    for (auto& th : Pool){
        if (th.joinable()){
            th.join();
        }
    }
}

void Workers::InitiateWorkers(std::function<void(Scheduler&)> task , Scheduler& scheduler){

    for (int i = 0 ; i < MaxPoolWorkers ; i++){
        Pool.emplace_back(task ,std::ref(scheduler));
    }
}

void Workers::Task(Scheduler& scheduler){

    while (1){
        auto Batch = scheduler.PullRequests();
        if (Batch == std::nullopt){
            break;
        }
        auto& ReqVec = Batch.value();
        // Fake backend : one call for the whole batch, a fixed overhead plus a
        // cost per request. This ratio is what makes batching pay off.
        std::this_thread::sleep_for(std::chrono::milliseconds(10 + ReqVec.size()));
        for (auto& Req : ReqVec){
            auto Id = Req.GetId();
            auto& input = Req.GetInput();
            std::vector<float> v;
            for (auto& e : input){
                v.push_back(e*56);
            }
            auto ResToVec = Response(Id , v);
            Req.SetValue(ResToVec);
        }  

    }
}



