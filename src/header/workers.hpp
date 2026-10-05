#pragma once
#include <vector>
#include <thread> 
#include <functional>


class Workers {
private:
    int MaxPoolWorkers;
    std::vector<std::thread> Pool;
public:

    Workers(int max);

    void InitiateWorkers(std::function<void()> task);

    
    

};