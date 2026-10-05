#include "workers.hpp"



Workers::Workers(int max):MaxPoolWorkers(max){};



void Workers::InitiateWorkers(std::function<void(void)> task){

    for (int i = 0 ; i < MaxPoolWorkers ; i++){
        Pool.emplace_back(task);
    }
}
