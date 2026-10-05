#pragma once

class Request{
private:
    int _id;
    int _model_id;
    
public:
    Request(int id , int model_id) : _id(id) , _model_id(model_id){}

    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    Request(Request&&) noexcept = default;
    Request& operator=(Request&&) noexcept = default;

    int GetId(){
        return _id;
       }
};