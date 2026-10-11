#pragma once
#include <cstddef>
#include <vector>
#include "request.hpp"

class Backend{
public:

    virtual std::vector<std::vector<float>> Run(const std::vector<Request>& Batch) = 0;
    virtual std::size_t InputSize() const = 0;
    virtual ~Backend() = default;  
};
