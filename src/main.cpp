#include "scheduler.hpp"
#include "workers.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

// Load generator : clients submit requests, wait for the response and record
// how long each one took. Every setting can be given on the command line so a
// trade-off can be explored without recompiling :
//
//   ./finton [workers] [batch_size] [delay_ms] [clients] [requests_per_client] [gap_ms]

using Clock = std::chrono::steady_clock;

struct ClientResult{
    std::vector<double> LatenciesMs;
    int Rejected = 0;   // queue full or server stopping
    int Dropped = 0;    // accepted, but the promise was destroyed without a value
    int WrongId = 0;    // response came back for another request
};

static int ArgOr(int argc, char** argv, int index, int fallback){
    return index < argc ? std::atoi(argv[index]) : fallback;
}

// Latencies must be sorted.
static double Percentile(const std::vector<double>& sorted, int percent){
    if (sorted.empty()) return 0;
    std::size_t index = sorted.size() * percent / 100;
    return sorted[std::min(index, sorted.size() - 1)];
}

int main(int argc, char** argv){
    const int NumWorkers        = ArgOr(argc, argv, 1, 2);
    const int MaxBatchSize      = ArgOr(argc, argv, 2, 4);
    const int MaxDelayMs        = ArgOr(argc, argv, 3, 5);
    const int NumClients        = ArgOr(argc, argv, 4, 4);
    const int RequestsPerClient = ArgOr(argc, argv, 5, 3);
    const int GapMs             = ArgOr(argc, argv, 6, 0);

    // The scheduler is declared before the workers : it must outlive the
    // threads that hold a reference to it (destruction is in reverse order).
    Scheduler scheduler(std::chrono::milliseconds(MaxDelayMs), MaxBatchSize, 1000);
    Workers workers(NumWorkers);
    workers.InitiateWorkers(
        [&workers](Scheduler& sch){ workers.Task(sch); },
        scheduler);

    // Each client keeps its own results, so no lock is needed while measuring.
    std::vector<ClientResult> Results(NumClients);
    std::vector<std::thread> Clients;

    auto Start = Clock::now();
    for (int c = 0; c < NumClients; c++){
        Clients.emplace_back([&scheduler, &Results, c, RequestsPerClient, GapMs]{
            ClientResult& Mine = Results[c];
            for (int j = 0; j < RequestsPerClient; j++){
                int Id = c * 1000 + j;

                // The future must be taken before the request is moved away.
                Request request(Id);
                std::future<Response> Future = request.GetFuture();

                auto Sent = Clock::now();
                if (!scheduler.SubmitRequest(std::move(request))){
                    Mine.Rejected++;
                    continue;
                }

                try{
                    Response response = Future.get();   // blocks until a worker answers
                    std::chrono::duration<double, std::milli> Latency = Clock::now() - Sent;
                    Mine.LatenciesMs.push_back(Latency.count());
                    if (response.request_id != Id) Mine.WrongId++;
                }catch (const std::future_error&){
                    Mine.Dropped++;   // abort shutdown dropped it
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(GapMs));
            }
        });
    }
    for (auto& client : Clients){
        client.join();
    }
    std::chrono::duration<double> Wall = Clock::now() - Start;

    scheduler.ForwardShutDown();

    std::vector<double> All;
    int Rejected = 0, Dropped = 0, WrongId = 0;
    for (auto& result : Results){
        All.insert(All.end(), result.LatenciesMs.begin(), result.LatenciesMs.end());
        Rejected += result.Rejected;
        Dropped += result.Dropped;
        WrongId += result.WrongId;
    }
    std::sort(All.begin(), All.end());

    std::cout << "\n--- settings" << std::endl;
    std::cout << "workers " << NumWorkers << " | batch size " << MaxBatchSize
              << " | delay " << MaxDelayMs << " ms | clients " << NumClients
              << " | requests/client " << RequestsPerClient
              << " | gap " << GapMs << " ms" << std::endl;

    std::cout << "--- results" << std::endl;
    std::cout << "completed  : " << All.size() << "/" << NumClients * RequestsPerClient
              << "  (rejected " << Rejected << ", dropped " << Dropped
              << ", wrong id " << WrongId << ")" << std::endl;
    std::cout << "wall time  : " << Wall.count() << " s" << std::endl;
    std::cout << "throughput : " << All.size() / Wall.count() << " requests/s" << std::endl;
    std::cout << "latency ms : p50 " << Percentile(All, 50)
              << " | p95 " << Percentile(All, 95)
              << " | p99 " << Percentile(All, 99)
              << " | max " << (All.empty() ? 0 : All.back()) << std::endl;

    bool Ok = All.size() == static_cast<std::size_t>(NumClients * RequestsPerClient) && WrongId == 0;
    return Ok ? 0 : 1;   // ~Workers joins the threads here
}
