#include "server.hpp"
#include "scheduler.hpp"
#include "workers.hpp"
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>

static Server* RunningServer = nullptr;

// Ctrl-C or kill : ask the server to stop, the real work happens in main
static void OnStopSignal(int){
    int saved = errno;
    if (RunningServer != nullptr){
        RunningServer->Stop();
    }
    errno = saved;
}

// Usage : finton [port]
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

    // Returns once the HTTP threads have answered everything they had received.
    // The workers must still be running during that time.
    int outcome = srv.StartServer();

    // Lets the workers leave their loop, otherwise ~Workers would join forever
    scheduler.ForwardShutDown();
    return outcome;
}
