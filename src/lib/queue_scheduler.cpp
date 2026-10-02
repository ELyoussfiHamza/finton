#include "header/queue_scheduler.hpp"
#include <iostream>

void Inference_Queue::enque_request(Request res){
    if (_strategy == QueuingStrategy::FIFO){
        std::cout << "Queued " << std::endl;
        q.push(res);
    }
}
