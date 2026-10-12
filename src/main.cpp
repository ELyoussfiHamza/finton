#include "server.hpp"
#include "onnxbackend.hpp"
#include "scheduler.hpp"
#include "workers.hpp"
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

static Server* RunningServer = nullptr;

static void OnStopSignal(int){
    int saved = errno;
    if (RunningServer != nullptr){
        RunningServer->Stop();
    }
    errno = saved;
}

static void PrintUsage(){
    std::cout <<
        "Usage : finton [options]\n"
        "  --port N           port to listen on (default 3001)\n"
        "  --model PATH       .onnx model to serve (default models/mnist/model.onnx)\n"
        "  --batch N          most requests in one batch (default 8)\n"
        "  --delay-ms N       longest time a request waits for its batch to fill (default 5)\n"
        "  --workers N        threads that run batches (default 2)\n"
        "  --threads N        threads ONNX Runtime uses inside one inference, 0 = its default (default 0)\n"
        "  --queue N          requests that can wait in the scheduler (default 64)\n"
        "  --http-threads N   threads that serve connections (default 64)\n";
}

int main(int argc, char** argv){
    ServerConfig config;
    config.Port = 3001;
    config.HeaderCap = 8000; // 8KB
    config.BodyCap = 10000000; // ~ 10 MB
    config.HttpThreads = 64;
    std::string ModelPath = "models/mnist/model.onnx";
    int MaxDelayMs = 5;
    int MaxBatchSize = 8;
    int MaxQueueSize = 64;
    int WorkerCount = 2;
    int InferenceThreads = 0;

    for (int i = 1 ; i < argc ; i++){
        std::string option = argv[i];
        if (option == "--help" || option == "-h"){
            PrintUsage();
            return 0;
        }
        if (i + 1 >= argc){
            std::cerr << "Missing value for " << option << std::endl;
            PrintUsage();
            return 1;
        }
        std::string value = argv[++i];
        if (option == "--model"){
            ModelPath = value;
            continue;
        }
        int number = 0;
        try{
            std::size_t used = 0;
            number = std::stoi(value, &used);
            if (used != value.size() || number < 0){
                throw std::invalid_argument(value);
            }
        }catch (const std::exception&){
            std::cerr << "Invalid value for " << option << " : " << value << std::endl;
            return 1;
        }
        if (option == "--port") config.Port = number;
        else if (option == "--batch") MaxBatchSize = number;
        else if (option == "--delay-ms") MaxDelayMs = number;
        else if (option == "--workers") WorkerCount = number;
        else if (option == "--threads") InferenceThreads = number;
        else if (option == "--queue") MaxQueueSize = number;
        else if (option == "--http-threads") config.HttpThreads = number;
        else{
            std::cerr << "Unknown option " << option << std::endl;
            PrintUsage();
            return 1;
        }
    }
    if (WorkerCount < 1 || MaxBatchSize < 1 || config.HttpThreads < 1 || MaxQueueSize < 1){
        std::cerr << "--workers, --batch, --queue and --http-threads must be at least 1" << std::endl;
        return 1;
    }

    std::unique_ptr<OnnxBackend> backend;
    try{
        backend = std::make_unique<OnnxBackend>(ModelPath, InferenceThreads);
    }catch (const std::exception& e){
        std::cerr << "Could not load the model " << ModelPath << " : " << e.what() << std::endl;
        return 1;
    }
    std::cout << "Model " << ModelPath << " | " << backend->Describe() << std::endl;
    std::cout << "Settings | batch " << MaxBatchSize << " | delay " << MaxDelayMs << " ms | workers " << WorkerCount
              << " | queue " << MaxQueueSize << " | http threads " << config.HttpThreads << std::endl;
    if (config.HttpThreads < MaxBatchSize * WorkerCount){
        std::cout << "Warning : fewer http threads than batch * workers, the batches cannot all fill" << std::endl;
    }
    config.InputSize = backend->InputSize();

    auto MaxDelay = std::chrono::milliseconds(MaxDelayMs);

    int outcome = 0;
    {
        Scheduler scheduler(MaxDelay, MaxBatchSize, MaxQueueSize);
        Workers workers(WorkerCount);
        workers.InitiateWorkers([&workers, &backend](Scheduler& s){ workers.Task(s, *backend); }, scheduler);

        Server srv(config, scheduler);

        RunningServer = &srv;
        struct sigaction action {};
        action.sa_handler = OnStopSignal;
        action.sa_flags = SA_RESTART;
        sigaction(SIGINT, &action, nullptr);
        sigaction(SIGTERM, &action, nullptr);

        outcome = srv.StartServer();

        RunningServer = nullptr;
        scheduler.ForwardShutDown();
    }

    if (backend->Batches() > 0){
        std::cout << "Served " << backend->Requests() << " requests in " << backend->Batches() << " batches | average batch size "
                  << static_cast<double>(backend->Requests()) / static_cast<double>(backend->Batches()) << std::endl;
    }
    return outcome;
}
