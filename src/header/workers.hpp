#pragma once
#include <vector>
#include <thread> 
#include <functional>
#include "scheduler.hpp"

class Workers {
private:
    int MaxPoolWorkers;
    std::vector<std::thread> Pool;
public:

    Workers(int max);

    ~Workers();
    void InitiateWorkers(std::function<void(Scheduler& , int)> task , Scheduler& scheduler );

    void Task(Scheduler& sch , int thread_id );


    
};