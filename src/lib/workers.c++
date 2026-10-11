#include "workers.hpp"
#include "scheduler.hpp"
#include <exception>
#include <iostream>
#include <thread>
#include <utility>


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

void Workers::Task(Scheduler& scheduler , Backend& backend){

    while (1){
        auto Batch = scheduler.PullRequests();
        if (Batch == std::nullopt){
            break;
        }
        auto& ReqVec = Batch.value();
        
        std::vector<std::vector<float>> Outputs;
        try{
            Outputs = backend.Run(ReqVec);
        }catch (const std::exception& e){
            std::cerr << "backend : " << e.what() << std::endl;
            continue;
        }
        if (Outputs.size() != ReqVec.size()){
            std::cerr << "backend : wrong number of outputs" << std::endl;
            continue;
        }
        for (std::size_t i = 0 ; i < ReqVec.size() ; i++){
            ReqVec[i].SetValue(Response{ReqVec[i].GetId() , std::move(Outputs[i])});
        }

    }
}



