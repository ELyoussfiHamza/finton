#include "onnxbackend.hpp"
#include "request.hpp"
#include <onnxruntime_cxx_api.h>
#include <string>
#include <utility>
#include <vector>


OnnxBackend::OnnxBackend(const std::string& path):Obj(ORT_LOGGING_LEVEL_WARNING,"hello"),OptObj(),LoadedModel(Obj,path.c_str(),OptObj){
    Shape = LoadedModel.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
};
std::vector<std::int64_t> OnnxBackend::GetShape() const{
    return  Shape;
}
std::vector<std::vector<float>> OnnxBackend::Run(const std::vector<Request>& Batch){
    // These model's specific params will be feeded from modelConfig later
    // Will hardcod them Now
    auto ImageSize = 784;
    std::vector<float> TensorInput;
    TensorInput.reserve(ImageSize *Batch.size());
    for (auto& req : Batch){
        const auto& _input = req.GetInput();
        TensorInput.insert(TensorInput.end(), _input.begin(), _input.end());
    }
    std::vector<int64_t> BatchShape{static_cast<int64_t>(Batch.size()),1,28,28};
    auto MemInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    auto Tensor = Ort::Value::CreateTensor<float>(MemInfo,TensorInput.data(),TensorInput.size(),BatchShape.data(),BatchShape.size());

    const char* InputNames[] {"Input3"};
    const char* OutputNames[] {"Plus214_Output_0"};    
    auto v = LoadedModel.Run(
        Ort::RunOptions{nullptr},
        InputNames,
        &Tensor,
        1,
        OutputNames,
        1
    );
    std::vector <std::vector<float>> Scores;
    auto Start = v[0].GetTensorData<float>();
    // We know there 10 * size_of_batch element 
    size_t i = 0;
    while (i<Batch.size()*10){
            std::vector<float> ReqOutput (Start , Start+10);
            Scores.push_back(std::move(ReqOutput));
            Start = Start + 10;
            i+=10;
    };
    return Scores;
}