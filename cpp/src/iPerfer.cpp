#include <spdlog/spdlog.h>
#include <cxxopts.hpp>

#include <string.h>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>

#include <chrono>
using clk = std::chrono::high_resolution_clock;

ssize_t receiveData(int fd, void * buffer, size_t len){
    uint8_t* p = static_cast<uint8_t*>(buffer);
    ssize_t received = 0;
    while (received < len){
        ssize_t n = recv(fd, p + received, len - received, 0);

        if (n == 0){
            return received;
        }
        if (n < 0){
            return -1;
        }
        received += n;
    }

    return received;
}

ssize_t sendData(int fd, const void * message, const ssize_t& size) {
    const uint8_t* buffer = static_cast<const uint8_t*>(message);

    ssize_t total_sent = 0;

	while (total_sent < size) {

		ssize_t sent = send(fd, buffer + total_sent, size - total_sent, MSG_NOSIGNAL);

		if (sent < 0) {
			return -1;
		}

		total_sent += sent;

		if (sent == 0) {
			break;
		}
	}
    return total_sent;
}

struct TimeMeasure { 
    uint64_t bytes = 0; 
    double secs = 0.0; 
};

TimeMeasure getTimeClient(int sock, double secs){
    //send the 80kb data shit here and measure the times
    const char A = 'A';
    static const size_t data = 80 * 1024; //80 KB size
    static std::vector<uint8_t> data_buffer(data, '\0');
    char ack = 0;

    uint64_t total = 0;
    auto t1 = clk::now();
    while (true){

        double elapsed = std::chrono::duration<double>(clk::now() - t1).count();
        if (elapsed >= secs){
            break;
        }

        if (sendData(sock, data_buffer.data(), data_buffer.size()) != (ssize_t)data_buffer.size()){
            spdlog::error("getTimeClient, data is not correctly sized");
        }
        total += data_buffer.size();

        if (receiveData(sock, &ack, 1) != 1 || ack != A){
            spdlog::error("getTimeClient failed");
        }

    }
    return {total, std::chrono::duration<double>(clk::now() - t1).count()};

}

TimeMeasure getTimeServer(int sock){
    const char A = 'A';
    static const size_t data = 80 * 1024; //80 KB size
    static std::vector<uint8_t> data_buffer(data);
    uint64_t total = 0;

    auto t1 = clk::now();
    while (true){
        ssize_t received = receiveData(sock, data_buffer.data(), data_buffer.size());
        if (received == 0){ //client is closed
            break;
        }

        if (received == (ssize_t)data){
            if (sendData(sock, &A, 1) != 1){
                spdlog::error("getTimeServer failure");
            } else{
                break; //client stopped mid-chunk??? actually don't think it's possible for this to happen
                // client will send entire chunk
            }
        }
    }
    auto t2 = clk::now();
    return {total, std::chrono::duration<double>(t2 - t1).count()};
}


ssize_t getRTTClient(int sock){
    const char M = 'M';
    char ack = 0;
    std::vector<int> samples;
    samples.reserve(8);
    for (int i = 0; i < 8; i++){
        auto t1 = clk::now();
        if (sendData(sock, &M, 1) != 1){
            spdlog::error("client: sending message failed");
        }
        if (receiveData(sock, &ack, 1) != 1 || ack != 'A'){
            spdlog::error("client: receiving ack failed");
        }
        auto t2 = clk::now();
        int rtt = (int)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        spdlog::info("sample is being taken");
        samples.push_back(rtt);
    }

    int numSamples = samples.size();
    int sum = 0;
    for (int i = numSamples - 4; i < numSamples; i++){
        sum += samples[i];
    }

    return sum/4;
}

ssize_t getRTTServer(int sock){
    char hold = 0;
    const char A = 'A';
    const char ack = '\0';
    std::vector<int> samples;
    samples.reserve(7);

    if (receiveData(sock, &hold, 1) != 1 || hold != 'M'){
        spdlog::error("server: receiving data failed");
    }
    auto tAck = clk::now();
    if (sendData(sock, &A, 1) != 1){
        spdlog::error("server: sending data failed");
    }


    for (int i = 1; i < 8; i++){
        spdlog::info("sample is being taken");
        auto t1 = clk::now();
        if (receiveData(sock, &hold, 1) != 1 || hold != 'M'){
            spdlog::error("server: receiving data failed");
        }
        auto tRcv = clk::now();
        int rtt = (int)std::chrono::duration_cast<std::chrono::milliseconds>(tRcv - tAck).count();
        samples.push_back(rtt);

        tAck = clk::now();
        if (sendData(sock, &A, 1) != 1){
            spdlog::error("server: sending message failed");
        }
    }

    int numSamples = samples.size();
    int sum = 0;
    for (int i = numSamples - 4; i < numSamples; i++){
        sum += samples[i];
    }

    return sum/4;
}


int runServer(int port){
    spdlog::info("We got here");
    
    // Make a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == -1)  {
        perror("error making socket"); //TODO: I don't know if i should be doing perror
        exit(1); //TODO: I don't know if i should be calling exit
    }

    // Option for allowing you to reuse socket
    int yes {1};
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("error setsockopt");
        exit(1);
    }

    // Bind socket to a port
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(sockfd, (sockaddr*)&addr, (socklen_t) sizeof(addr)) == -1) {
        perror("error binding socket");
        exit(1);
    }

    // Listen for incoming connections
    if (listen(sockfd, 10) == -1) {
        perror("error listening");
        exit(1);
    }

    spdlog::info("iPerfer server started");
    while (true) {
        // Accept new connection
        struct sockaddr_in connection;
        socklen_t size = sizeof(connection);
        int connectionfd = accept(sockfd, (struct sockaddr*)&connection, &size);
        if (connectionfd < 0){
            continue;
        }
        spdlog::info("Server RTT {}" , getRTTServer(connectionfd));
        TimeMeasure hold = getTimeServer(connectionfd);
        spdlog::info("Server Big Data {}", hold.bytes);
        close(connectionfd);
        break;
    }

    close(sockfd);

    return 0;
}

int runClient(const char * hostName, int port, double time){
    spdlog::info("We got here too gang");

    // Make a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == -1)  {
        perror("error making socket");
        exit(1);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    struct hostent *host = gethostbyname(hostName);
    if (host == NULL) {
        perror("error gethostbyname");
        exit(1);
    }
    memcpy(&addr.sin_addr, host->h_addr, host->h_length);
    addr.sin_port = htons(port);  // server port

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("error connecting");
        exit(1);
    }

    //Client should send eight 1-byte packets to server to estimate RTT, waiting for ACK between each one
    // After RTT estimation, client should send data for duration of "time" argument

    spdlog::info("RTTCLIENT RTT: {}", getRTTClient(sockfd));
    TimeMeasure hold = getTimeClient(sockfd, time);
    spdlog::info("RTTCLIENT client data: {}", hold.bytes);

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);

    return 0;
}

int main(int argc, char *argv[])
{
    cxxopts::Options options("iPerfer", "A simple network performance measurement tool");
    options.add_options()
        ("s, server", "Enable server", cxxopts::value<bool>()->default_value("false"))
        ("p, port", "Port number to use", cxxopts::value<int>()->default_value("0"))
        ("c,client", "Enable client", cxxopts::value<bool>()->default_value("false"))
        ("h,host", "Server hostname (for client mode)", cxxopts::value<std::string>()->default_value(""))
        ("t,time", "Test duration in seconds (for client mode)", cxxopts::value<double>()->default_value("0"));

    auto result = options.parse(argc, argv);

    auto is_server = result["server"].as<bool>();
    auto is_client = result["client"].as<bool>();
    auto port = result["port"].as<int>();

    if (is_server == is_client){
        spdlog::error("Must specify one of either server or client, not both");
        return 1;
    }

    spdlog::debug("About to check port number...");
    if (port < 1024 || port > 0xFFFF) {
        spdlog::error("Port number should be in interval [1024, 65535]; instead received {}", port);
        return 1;
    }

    if (is_server) {
        //run some server code
        spdlog::info("iPerfer server started on port {}", port);
        runServer(port);
    } else{
        auto host = result["host"].as<std::string>();
        spdlog::debug("About to check host value ...");
        if (host.empty()) {
            spdlog::error("Error: host value does not exist");
            return 1;
        }

        auto time = result["time"].as<double>();
        spdlog::debug("About to check time value ...");
        if (time <= 0) {
            spdlog::error("Error: time argument must be greater than 0");
            return 1;
        }
        spdlog::info("iPerfer client connected");
        runClient(host.c_str(), port, time);
    }
    
}