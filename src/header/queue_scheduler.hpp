#pragma once

// This is the queue of requests and scheduler of next processable request
#include <queue>
// We define a custom request type that will wrapp the c++ grpc/http request later
class Request{
private:
    int _id;

public:
    Request(int id) {}
    
    int getId(){};
};

enum class QueuingStrategy{
    FIFO
};


// The custom type of queue of requests 
class Inference_Queue{

private:
QueuingStrategy _strategy;
std::queue<Request> q;

public:

void enque_request(Request req ) {};

Request yield_request(){};

};

