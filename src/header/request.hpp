#pragma once
#include <future>
#include <chrono>
#include <vector>
struct Response{
    int request_id;
    std::vector<float> result; 
};

class Request{
private:
    int _id;
    std::promise<Response> promise;
    std::chrono::steady_clock::time_point arrival; 
    std::vector<float> input;
public:
    
    Request(int id, std::vector<float> _input) : _id(id), arrival(std::chrono::steady_clock::now()) ,input(std::move(_input)) {}
    
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    Request(Request&&) noexcept = default;
    Request& operator=(Request&&) noexcept = default;

    int GetId()const {
        return _id;
       };
    std::chrono::steady_clock::time_point GetArrival() const{
        return arrival;
    };
    void SetValue(Response res) {
        promise.set_value(std::move(res));
    };

    std::future<Response> GetFuture()  {
        return promise.get_future();
    };

    const std::vector<float>& GetInput(){
        return input;
    };
    
};