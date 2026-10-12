#include "onnxbackend.hpp"
#include "request.hpp"
#include <onnxruntime_cxx_api.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>


static Ort::SessionOptions MakeOptions(int Threads){
    Ort::SessionOptions Options;
    if (Threads > 0){
        Options.SetIntraOpNumThreads(Threads);
    }
    return Options;
}

OnnxBackend::OnnxBackend(const std::string& path, int Threads)
    :Obj(ORT_LOGGING_LEVEL_WARNING,"finton"),OptObj(MakeOptions(Threads)),LoadedModel(Obj,path.c_str(),OptObj),
     MemInfo(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)),Threads(Threads){
    if (LoadedModel.GetInputCount() != 1 || LoadedModel.GetOutputCount() != 1){
        throw std::runtime_error("finton supports models with exactly one input and one output, this one has " +
                                 std::to_string(LoadedModel.GetInputCount()) + " input(s) and " +
                                 std::to_string(LoadedModel.GetOutputCount()) + " output(s)");
    }

    Ort::AllocatorWithDefaultOptions Allocator;
    InputName = LoadedModel.GetInputNameAllocated(0,Allocator).get();
    OutputName = LoadedModel.GetOutputNameAllocated(0,Allocator).get();

    auto InputInfo = LoadedModel.GetInputTypeInfo(0);
    auto OutputInfo = LoadedModel.GetOutputTypeInfo(0);
    if (InputInfo.GetONNXType() != ONNX_TYPE_TENSOR || OutputInfo.GetONNXType() != ONNX_TYPE_TENSOR){
        throw std::runtime_error("finton supports models whose input and output are tensors");
    }
    auto InputTensorInfo = InputInfo.GetTensorTypeAndShapeInfo();
    if (InputTensorInfo.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
        OutputInfo.GetTensorTypeAndShapeInfo().GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT){
        throw std::runtime_error("finton supports models whose input and output are 32-bit floats");
    }

    Shape = InputTensorInfo.GetShape();
    if (Shape.empty()){
        throw std::runtime_error("the input of the model has no dimensions");
    }
    if (Shape[0] == -1){
        Batchable = true;
    }else if (Shape[0] != 1){
        throw std::runtime_error("the first dimension of the input must be the batch size : flexible, or 1. This model has " +
                                 std::to_string(Shape[0]));
    }
    for (std::size_t i = 1 ; i < Shape.size() ; i++){
        if (Shape[i] <= 0){
            throw std::runtime_error("only the first dimension of the input can be flexible, dimension " +
                                     std::to_string(i) + " is not fixed");
        }
        InputElements *= static_cast<std::size_t>(Shape[i]);
    }
};

std::vector<std::int64_t> OnnxBackend::GetShape() const{
    return  Shape;
}

std::size_t OnnxBackend::InputSize() const{
    return InputElements;
}

bool OnnxBackend::CanBatch() const{
    return Batchable;
}

std::string OnnxBackend::Describe() const{
    std::string text = "input " + InputName + " : float32 [";
    for (std::size_t i = 0 ; i < Shape.size() ; i++){
        text += (i == 0 ? "" : ", ") + (Shape[i] == -1 ? std::string("N") : std::to_string(Shape[i]));
    }
    text += "], " + std::to_string(InputElements) + " floats per request | output " + OutputName +
            " | batching " + (Batchable ? "yes" : "no, one call per request") +
            " | threads per inference " + (Threads > 0 ? std::to_string(Threads) : std::string("default"));
    return text;
}

std::uint64_t OnnxBackend::Batches() const{
    return BatchCount;
}

std::uint64_t OnnxBackend::Requests() const{
    return RequestCount;
}

void OnnxBackend::RunTensor(const float* Data, std::size_t Count, std::vector<std::vector<float>>& Outputs){
    std::vector<std::int64_t> BatchShape = Shape;
    BatchShape[0] = static_cast<std::int64_t>(Count);
    auto Tensor = Ort::Value::CreateTensor<float>(MemInfo,const_cast<float*>(Data),Count * InputElements,BatchShape.data(),BatchShape.size());

    const char* InputNames[] {InputName.c_str()};
    const char* OutputNames[] {OutputName.c_str()};
    auto v = LoadedModel.Run(
        Ort::RunOptions{nullptr},
        InputNames,
        &Tensor,
        1,
        OutputNames,
        1
    );
    std::size_t Total = v[0].GetTensorTypeAndShapeInfo().GetElementCount();
    if (Total % Count != 0){
        throw std::runtime_error("the output of the model cannot be split per request");
    }
    std::size_t PerRequest = Total / Count;
    auto Start = v[0].GetTensorData<float>();
    for (std::size_t i = 0 ; i < Count ; i++){
        Outputs.emplace_back(Start , Start + PerRequest);
        Start = Start + PerRequest;
    }
}

std::vector<std::vector<float>> OnnxBackend::Run(const std::vector<Request>& Batch){
    std::vector <std::vector<float>> Scores;
    if (Batch.empty()){
        return Scores;
    }
    Scores.reserve(Batch.size());
    BatchCount++;
    RequestCount += Batch.size();
    for (auto& req : Batch){
        if (req.GetInput().size() != InputElements){
            throw std::runtime_error("a request does not have the input size of the model");
        }
    }

    if (!Batchable){
        for (auto& req : Batch){
            RunTensor(req.GetInput().data(), 1, Scores);
        }
        return Scores;
    }

    std::vector<float> TensorInput;
    TensorInput.reserve(InputElements * Batch.size());
    for (auto& req : Batch){
        const auto& _input = req.GetInput();
        TensorInput.insert(TensorInput.end(), _input.begin(), _input.end());
    }
    RunTensor(TensorInput.data(), Batch.size(), Scores);
    return Scores;
}
