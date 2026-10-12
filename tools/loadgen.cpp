#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

using Clock = std::chrono::steady_clock;

struct Options{
    std::string Host = "127.0.0.1";
    int Port = 3001;
    int Clients = 16;
    double Seconds = 5;
    double Warmup = 1;
    int Floats = 784;
    bool Csv = false;
};

struct ClientResult{
    std::vector<double> LatenciesMs;
    std::uint64_t Ok = 0;
    std::uint64_t Rejected = 0;
    std::uint64_t Failed = 0;
};

static void PrintUsage(){
    std::cout <<
        "Usage : loadgen [options]\n"
        "  --host IP        server address (default 127.0.0.1)\n"
        "  --port N         server port (default 3001)\n"
        "  --clients N      connections sending requests at the same time (default 16)\n"
        "  --seconds N      how long to measure (default 5)\n"
        "  --warmup N       seconds sent before measuring starts (default 1)\n"
        "  --floats N       floats in each request, the input size of the model (default 784)\n"
        "  --csv            print one line : clients,seconds,ok,rejected,failed,throughput,mean,p50,p95,p99,max\n"
        "Each client sends a request, waits for the response, then sends the next one.\n";
}

static int Connect(const Options& options){
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1){
        return -1;
    }
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    struct timeval timeout {};
    timeout.tv_sec = 10;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(options.Port);
    if (inet_pton(AF_INET, options.Host.c_str(), &address.sin_addr) != 1 ||
        connect(fd, (struct sockaddr*)&address, sizeof(address)) == -1){
        close(fd);
        return -1;
    }
    return fd;
}

static bool SendAll(int fd, const std::string& data){
    std::size_t sent = 0;
    while (sent < data.size()){
        ssize_t n = send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n == -1){
            if (errno == EINTR){
                continue;
            }
            return false;
        }
        sent += n;
    }
    return true;
}

static int ReadResponse(int fd, std::string& buffer){
    char chunk[16384];
    while (true){
        auto head_end = buffer.find("\r\n\r\n");
        if (head_end != std::string::npos){
            std::string head = buffer.substr(0, head_end);
            std::transform(head.begin(), head.end(), head.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            std::size_t length = 0;
            auto at = head.find("content-length:");
            if (at != std::string::npos){
                length = std::strtoull(head.c_str() + at + 15, nullptr, 10);
            }
            std::size_t total = head_end + 4 + length;
            if (buffer.size() >= total){
                int status = head.size() >= 12 ? std::atoi(head.c_str() + 9) : 0;
                bool closing = head.find("connection: close") != std::string::npos;
                buffer.erase(0, total);
                return closing ? -status : status;
            }
        }
        ssize_t n = read(fd, chunk, sizeof(chunk));
        if (n == 0){
            return 0;
        }
        if (n == -1){
            if (errno == EINTR){
                continue;
            }
            return 0;
        }
        buffer.append(chunk, n);
    }
}

static void RunClient(const Options& options, const std::string& request, Clock::time_point measure_from,
                      Clock::time_point stop_at, ClientResult& result){
    int fd = -1;
    std::string buffer;
    while (Clock::now() < stop_at){
        if (fd == -1){
            fd = Connect(options);
            buffer.clear();
            if (fd == -1){
                result.Failed++;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
        }
        auto start = Clock::now();
        int status = SendAll(fd, request) ? ReadResponse(fd, buffer) : 0;
        auto end = Clock::now();
        bool closing = status <= 0;
        status = std::abs(status);
        if (closing){
            close(fd);
            fd = -1;
        }
        if (start < measure_from){
            continue;
        }
        if (status == 200){
            result.Ok++;
            result.LatenciesMs.push_back(std::chrono::duration<double, std::milli>(end - start).count());
        }else if (status == 503){
            result.Rejected++;
        }else{
            result.Failed++;
        }
    }
    if (fd != -1){
        close(fd);
    }
}

static double Percentile(const std::vector<double>& sorted, double p){
    if (sorted.empty()){
        return 0;
    }
    std::size_t index = static_cast<std::size_t>(std::ceil(p * sorted.size())) - 1;
    return sorted[std::min(index, sorted.size() - 1)];
}

int main(int argc, char** argv){
    Options options;
    for (int i = 1 ; i < argc ; i++){
        std::string option = argv[i];
        if (option == "--help" || option == "-h"){
            PrintUsage();
            return 0;
        }
        if (option == "--csv"){
            options.Csv = true;
            continue;
        }
        if (i + 1 >= argc){
            std::cerr << "Missing value for " << option << std::endl;
            return 1;
        }
        std::string value = argv[++i];
        if (option == "--host") options.Host = value;
        else if (option == "--port") options.Port = std::atoi(value.c_str());
        else if (option == "--clients") options.Clients = std::atoi(value.c_str());
        else if (option == "--seconds") options.Seconds = std::atof(value.c_str());
        else if (option == "--warmup") options.Warmup = std::atof(value.c_str());
        else if (option == "--floats") options.Floats = std::atoi(value.c_str());
        else{
            std::cerr << "Unknown option " << option << std::endl;
            PrintUsage();
            return 1;
        }
    }
    if (options.Clients < 1 || options.Floats < 0 || options.Seconds <= 0){
        std::cerr << "--clients must be at least 1 and --seconds above 0" << std::endl;
        return 1;
    }

    std::vector<float> input(options.Floats);
    for (int i = 0 ; i < options.Floats ; i++){
        input[i] = std::sin(0.01f * i) * 0.5f + 0.5f;
    }
    std::string request = "POST /infer HTTP/1.1\r\nHost: " + options.Host + "\r\n"
                          "Content-Type: application/octet-stream\r\n"
                          "Content-Length: " + std::to_string(input.size() * sizeof(float)) + "\r\n\r\n";
    request.append(reinterpret_cast<const char*>(input.data()), input.size() * sizeof(float));

    auto begin = Clock::now();
    auto measure_from = begin + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(options.Warmup));
    auto stop_at = measure_from + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(options.Seconds));

    std::vector<ClientResult> results(options.Clients);
    std::vector<std::thread> threads;
    for (int i = 0 ; i < options.Clients ; i++){
        threads.emplace_back(RunClient, std::cref(options), std::cref(request), measure_from, stop_at, std::ref(results[i]));
    }
    for (auto& thread : threads){
        thread.join();
    }

    std::vector<double> latencies;
    std::uint64_t ok = 0, rejected = 0, failed = 0;
    for (auto& result : results){
        ok += result.Ok;
        rejected += result.Rejected;
        failed += result.Failed;
        latencies.insert(latencies.end(), result.LatenciesMs.begin(), result.LatenciesMs.end());
    }
    std::sort(latencies.begin(), latencies.end());
    double mean = 0;
    for (double value : latencies){
        mean += value;
    }
    mean = latencies.empty() ? 0 : mean / latencies.size();
    double throughput = ok / options.Seconds;
    double p50 = Percentile(latencies, 0.50), p95 = Percentile(latencies, 0.95), p99 = Percentile(latencies, 0.99);
    double max = latencies.empty() ? 0 : latencies.back();

    std::cout << std::fixed << std::setprecision(2);
    if (options.Csv){
        std::cout << options.Clients << "," << options.Seconds << "," << ok << "," << rejected << "," << failed << ","
                  << throughput << "," << mean << "," << p50 << "," << p95 << "," << p99 << "," << max << std::endl;
    }else{
        std::cout << "clients " << options.Clients << " | " << options.Seconds << " s measured\n"
                  << "completed  : " << ok << "  (rejected with 503 : " << rejected << ", failed : " << failed << ")\n"
                  << "throughput : " << throughput << " requests/s\n"
                  << "latency ms : mean " << mean << " | p50 " << p50 << " | p95 " << p95 << " | p99 " << p99
                  << " | max " << max << std::endl;
    }
    return ok > 0 ? 0 : 1;
}
