#include "server.hpp"
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <system_error>
#include <vector>

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

static const char* Reason(int status){
    switch (status){
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 413: return "Content Too Large";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 503: return "Service Unavailable";
        default:  return "Unknown";
    }
}

static bool SendResponse(int fd, int status, const std::string& body, bool keep_alive,
                         const std::string& content_type = "application/json"){
    std::string response = "HTTP/1.1 " + std::to_string(status) + " " + Reason(status) + "\r\n"
                           "Content-Type: " + content_type + "\r\n"
                           "Content-Length: " + std::to_string(body.size()) + "\r\n"
                           "Connection: " + (keep_alive ? "keep-alive" : "close") + "\r\n\r\n" + body;
    return SendAll(fd, response);
}

static bool WantsKeepAlive(const HttpRequest& http){
    std::string connection = http.Header("connection").value_or("");
    std::transform(connection.begin(), connection.end(), connection.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    if (http.version == "HTTP/1.0"){
        return connection == "keep-alive";
    }
    return connection != "close";
}

Server::Server(const ServerConfig& config , Scheduler& scheduler)
    : Config(config) , MainScheduler(scheduler) , StopFd(eventfd(0, 0)){
    if (StopFd == -1){
        throw std::system_error(errno, std::generic_category(), "eventfd");
    }
}

Server::~Server(){
    close(StopFd);
}

void Server::Stop(){
    std::uint64_t one = 1;
    [[maybe_unused]] ssize_t n = write(StopFd, &one, sizeof(one));
}

int Server::ReadRequest(int client_fd, std::string& raw, HttpRequest& request){
    char buffer[16384];
    while (true){
        if (!raw.empty()){
            try{
                request = ParseHttpRequest(raw, Config.HeaderCap, Config.BodyCap);
                raw.erase(0, raw.find("\r\n\r\n") + 4 + request.body.size());
                return 200;
            }catch (const HttpIncompleteError&){
            }catch (const HttpHeaderMax& e){
                std::cerr << e.what() << std::endl;
                return 431;
            }catch (const HttpBodyMax& e){
                std::cerr << e.what() << std::endl;
                return 413;
            }catch (const HttpParseError& e){
                std::cerr << e.what() << std::endl;
                return 400;
            }
        }

        struct pollfd fds[2] = {{client_fd, POLLIN, 0}, {StopFd, POLLIN, 0}};
        int watched = raw.empty() ? 2 : 1;
        int ready = poll(fds, watched, Config.IdleTimeoutSeconds * 1000);
        if (ready == -1){
            if (errno == EINTR){
                continue;
            }
            std::cerr << "poll : " << std::strerror(errno) << std::endl;
            return 0;
        }
        if (ready == 0){
            if (!raw.empty()){
                std::cerr << "Client too slow. Request incomplete , therefore dropped!" << std::endl;
            }
            return 0;
        }
        if (fds[0].revents == 0){
            return 0;
        }

        ssize_t n = read(client_fd, buffer, sizeof(buffer));
        if (n == 0){
            if (!raw.empty()){
                std::cerr << "Client closed. Request incomplete , therefore dropped!" << std::endl;
            }
            return 0;
        }else if (n == -1){
            if (errno == EINTR){
                continue;
            }
            std::cerr << "read : " << std::strerror(errno) << std::endl;
            return 0;
        }
        raw.append(buffer, n);
    }
}

Server::Answer Server::Respond(const HttpRequest& http){
    if (http.method == "GET" && http.path == "/health"){
        return {200, R"({"status":"ok"})"};
    }
    if (http.method == "POST" && http.path == "/infer"){
        if (http.body.size() % sizeof(float) != 0){
            return {400, ""};
        }
        std::vector<float> input(http.body.size() / sizeof(float));
        std::memcpy(input.data(), http.body.data(), http.body.size());
        Request request(NextRequestId++, std::move(input));
        auto future = request.GetFuture();
        if (!MainScheduler.SubmitRequest(std::move(request))){
            return {503, ""};
        }
        Response result = future.get();
        std::string output(result.result.size() * sizeof(float), '\0');
        std::memcpy(output.data(), result.result.data(), output.size());
        return {200, std::move(output), "application/octet-stream"};
    }
    return {404, ""};
}

void Server::HandleConnection(int client_fd){
    struct timeval timeout {};
    timeout.tv_sec = Config.IdleTimeoutSeconds;
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    try{
        std::string raw;
        bool keep_alive = true;
        while (keep_alive){
            HttpRequest http;
            int status = ReadRequest(client_fd, raw, http);
            if (status == 0){
                break;
            }
            Answer answer{status, ""};
            if (status == 200){
                answer = Respond(http);
            }
            keep_alive = status == 200 && WantsKeepAlive(http) && !Stopping;
            if (!SendResponse(client_fd, answer.status, answer.body, keep_alive, answer.content_type)){
                break;
            }
        }
    }catch (const std::exception& e){
        std::cerr << "connection : " << e.what() << std::endl;
        SendResponse(client_fd, 500, "", false);
    }catch (...){
        SendResponse(client_fd, 500, "", false);
    }
    close(client_fd);
}

void Server::ConnectionLoop(){
    while (true){
        int client_fd;
        {
            std::unique_lock<std::mutex> lock(PendingMutex);
            PendingCv.wait(lock, [this]{ return !Pending.empty() || Draining; });
            if (Pending.empty()){
                return;
            }
            client_fd = Pending.front();
            Pending.pop();
        }
        HandleConnection(client_fd);
    }
}

int Server::StartServer(){

    // Create file descriptor (a socket ): a way to communicate 
    // We use TCP / ipv4
    int ServerSocket = socket(AF_INET , SOCK_STREAM , 0);

    if (ServerSocket == -1){
        std::cerr << std::strerror(errno) << std::endl;
        return 1;
    }
    // In case server closed and we reconnect we can reuse the port 
    // we set  the option to 1 which means true we want that feature:port-reuse
    int opt = 1;
    setsockopt(ServerSocket , SOL_SOCKET , SO_REUSEADDR,&opt,sizeof(opt));

    //Hna: Here we setup server address 
    struct sockaddr_in server_address {};
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    // htons is used to do the proper bytes formatting
    server_address.sin_port = htons(Config.Port);

    if (bind(ServerSocket , (struct sockaddr*)& server_address , sizeof(server_address)) == -1){
        std::cerr << std::strerror(errno) << std::endl;
        close(ServerSocket);
        return 1;
    }

    //Listen for connections
    if (listen(ServerSocket , Config.Backlog) < 0){
        std::cerr << std::strerror(errno) << std::endl;
        close(ServerSocket);
        return 1;
    }

    for (int i = 0 ; i < Config.HttpThreads ; i++){
        HttpThreads.emplace_back(&Server::ConnectionLoop, this);
    }

    std::cout << "Your wonderful hand written lol server is listening on " << Config.Port << std::endl;
    //accept loop
    struct pollfd fds[2] = {{ServerSocket, POLLIN, 0}, {StopFd, POLLIN, 0}};
    while (true){
        if (poll(fds, 2, -1) == -1){
            if (errno == EINTR){
                continue;
            }
            std::cerr << "poll : " << std::strerror(errno) << std::endl;
            break;
        }
        if (fds[1].revents != 0){
            break;
        }

        int client_fd = accept(ServerSocket, nullptr, nullptr);
        if (client_fd < 0) {
            std::cerr << std::strerror(errno) << std::endl;
            continue;
        }

        bool queued = false;
        {
            std::unique_lock<std::mutex> lock(PendingMutex);
            if (Pending.size() < static_cast<std::size_t>(Config.MaxPendingConnections)){
                Pending.push(client_fd);
                queued = true;
            }
        }
        if (queued){
            PendingCv.notify_one();
        }else{
            SendResponse(client_fd, 503, "", false);
            close(client_fd);
        }
    }

    std::cout << "Stopping : finishing the requests in progress" << std::endl;
    close(ServerSocket);
    Stopping = true;
    {
        std::unique_lock<std::mutex> lock(PendingMutex);
        Draining = true;
    }
    PendingCv.notify_all();
    for (auto& th : HttpThreads){
        th.join();
    }
    HttpThreads.clear();
    return 0;
}
