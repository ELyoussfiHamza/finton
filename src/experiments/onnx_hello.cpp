#include <onnxruntime_c_api.h>
#include <onnxruntime_cxx_api.h>
#include <iostream>
#include <vector>

int main(){

    try{
        Ort::Env Obj(ORT_LOGGING_LEVEL_WARNING,"hello");
        Ort::SessionOptions OptObj;
        Ort::Session LoadedModel(Obj,"models/mnist/model.onnx",OptObj);
        std::vector<float> Data(784,0.0f);
        std::vector<int64_t> Shape{1,1,28,28};

        auto MemInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        auto Tensor = Ort::Value::CreateTensor<float>(MemInfo,Data.data(),Data.size(),Shape.data(),Shape.size());

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
        auto TensorPointer = v[0].GetTensorData<float>();


        int i = 0;
        while (i<10){
            std::cout << *(TensorPointer+i) << std::endl;
            i++;
        };
        
    }catch(const Ort::Exception& e){
        std::cerr << e.what() << std::endl;
        return 1;
    }


    return 0;
}

