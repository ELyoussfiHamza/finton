#pragma once
#include "backend.hpp"
#include <cstdint>
#include <onnxruntime_cxx_api.h>
#include <string>

class OnnxBackend : public Backend{
    private:
        Ort::Env Obj;
        Ort::SessionOptions OptObj;
        Ort::Session LoadedModel;
        std::vector<std::int64_t> Shape;
    public:
        OnnxBackend(const std::string& path);
        std::vector<std::vector<float>> Run(const std::vector<Request>& Batch) override;
        std::vector<std::int64_t> GetShape()const;
};