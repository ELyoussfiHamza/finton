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

void Workers::InitiateWorkers(std::function<void(Scheduler&)> task , Scheduler& scheduler){

    for (int i = 0 ; i < MaxPoolWorkers ; i++){
        Pool.emplace_back(task ,std::ref(scheduler));
    }
}

void Workers::Task(Scheduler& scheduler){

    while (1){
        auto Batch = scheduler.PullRequests();
        if (Batch == std::nullopt){
            std::cout << "Likely shutdown" << std::endl;
            break;
        }
        auto& ReqVec = Batch.value();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        for (int i = 0 ;i< ReqVec.size() ; i++){
            auto Id = ReqVec[i].GetId();
            auto ResToVec = Response(Id , i);
            ReqVec[i].SetValue(ResToVec);
        }  

    }
}



