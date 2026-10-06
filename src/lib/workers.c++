#include "workers.hpp"
#include "scheduler.hpp"
#include <iostream>
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

void Workers::InitiateWorkers(std::function<void(Scheduler& , int)> task , Scheduler& scheduler){

    for (int i = 0 ; i < MaxPoolWorkers ; i++){
        Pool.emplace_back(task ,std::ref(scheduler),i);
    }
}

void Workers::Task(Scheduler& scheduler , int thread_int){

    while (1){
        auto Batch = scheduler.PullRequests();
        if (Batch == std::nullopt){
            std::cout << "Likely shutdown" << std::endl;
            break;
        }
        auto& ReqVec = Batch.value();
        for (int i = 0 ;i< ReqVec.size() ; i++){
            auto Id = ReqVec[i].GetId();
            std::cout <<"Processing " << Id << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
    }
}

