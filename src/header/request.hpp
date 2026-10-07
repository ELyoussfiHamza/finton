#pragma once
#include <future>
#include <chrono>

struct Response{
    int request_id;
    int result; 
};

class Request{
private:
    int _id;
    std::promise<Response> promise;
    std::chrono::steady_clock::time_point arrival; 
public:

    Request(int id) : _id(id), arrival(std::chrono::steady_clock::now()) {}

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
        promise.set_value(res);
    };

    std::future<Response> GetFuture()  {
        return promise.get_future();
    }
};