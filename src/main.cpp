#include "server.hpp"
#include "onnxbackend.hpp"
#include "scheduler.hpp"
#include "workers.hpp"
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
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

int main(int argc, char** argv){
    ServerConfig config;
    config.Port = argc > 1 ? std::atoi(argv[1]) : 3001;
    config.HeaderCap = 8000; // 8KB
    config.BodyCap = 10000000; // ~ 10 MB
    config.HttpThreads = 64;
    std::string ModelPath = argc > 2 ? argv[2] : "models/mnist/model.onnx";

    std::unique_ptr<OnnxBackend> backend;
    try{
        backend = std::make_unique<OnnxBackend>(ModelPath);
    }catch (const Ort::Exception& e){
        std::cerr << "Could not load the model : " << e.what() << std::endl;
        return 1;
    }
    config.InputSize = 1;
    auto InputShape = backend->GetShape();
    for (std::size_t i = 1 ; i < InputShape.size() ; i++){
        config.InputSize *= static_cast<std::size_t>(InputShape[i]);
    }

    auto MaxDelay = std::chrono::milliseconds(5);
    int MaxBatchSize = 8;
    int MaxQueueSize = 64;
    int WorkerCount = 2;

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

    int outcome = srv.StartServer();

    scheduler.ForwardShutDown();
    return outcome;
}
