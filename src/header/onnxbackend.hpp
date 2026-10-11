#pragma once
#include "backend.hpp"
#include <cstddef>
#include <cstdint>
#include <onnxruntime_cxx_api.h>
#include <string>
#include <vector>

class OnnxBackend : public Backend{
    private:
        Ort::Env Obj;
        Ort::SessionOptions OptObj;
        Ort::Session LoadedModel;
        Ort::MemoryInfo MemInfo;
        std::vector<std::int64_t> Shape;
        std::string InputName;
        std::string OutputName;
        std::size_t InputElements = 1;
        bool Batchable = false;

        void RunTensor(const float* Data, std::size_t Count, std::vector<std::vector<float>>& Outputs);
    public:
        OnnxBackend(const std::string& path);
        std::vector<std::vector<float>> Run(const std::vector<Request>& Batch) override;
        std::size_t InputSize() const override;
        std::vector<std::int64_t> GetShape()const;
        bool CanBatch() const;
        std::string Describe() const;
};
