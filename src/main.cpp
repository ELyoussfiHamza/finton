#include "server.hpp"
#include "scheduler.hpp"
#include "workers.hpp"
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>

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

    auto MaxDelay = std::chrono::milliseconds(5);
    int MaxBatchSize = 8;
    int MaxQueueSize = 64;
    int WorkerCount = 2;

    Scheduler scheduler(MaxDelay, MaxBatchSize, MaxQueueSize);
    Workers workers(WorkerCount);
    workers.InitiateWorkers([&workers](Scheduler& s){ workers.Task(s); }, scheduler);

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
