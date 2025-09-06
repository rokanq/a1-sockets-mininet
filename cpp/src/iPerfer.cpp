#include <spdlog/spdlog.h>
#include <cxxopts.hpp>

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>

#define PORT 8080

auto sendData(int fd, const char * message, const ssize_t& size) -> void{
    ssize_t total_sent = 0;
	while (total_sent < size) {
		ssize_t sent = send(fd, message + total_sent, size - total_sent, MSG_NOSIGNAL);

		if (sent < 0) {
			return -1;
		}

		total_sent += sent;

		if (sent == 0) {
			break;
		}
	}
}

int getRTTClient(int sock){
    const char M = 'M';

    for (int i = 0; i < 8; i++){

    }
}

int runServer(int port){
    spdlog::info("We got here");
    
    // Make a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == -1)  {
        perror("error making socket");
        exit(1);
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
    addr.sin_port = htons(PORT);
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

        // Print what IP address connection is from
        char s[INET6_ADDRSTRLEN];
        printf("connetion from %s\n", inet_ntoa(connection.sin_addr));

        // Receive message
        char buf[1024];
        int ret {};
        if ((ret = recv(connectionfd, buf, sizeof(buf), 0)) == -1) {
            perror("recv");
            close(connectionfd);
            continue;
        }
        buf[ret] = '\0';

        printf("message: %s\n", buf);
        close(connectionfd);
    }

    close(sockfd);

    return 0;
}

int runClient(){
    spdlog::info("We got here too gang");

    // Make a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == -1)  {
        perror("error making socket");
        exit(1);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    struct hostent *host = gethostbyname("127.0.0.1");
    if (host == NULL) {
        perror("error gethostbyname");
        exit(1);
    }
    memcpy(&addr.sin_addr, host->h_addr, host->h_length);
    addr.sin_port = htons(8080);  // server port

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("error connecting");
        exit(1);
    }

    //Client should send eight 1-byte packets to server to estimate RTT, waiting for ACK between each one
    // After RTT estimation, client should send data for duration of "time" argument

    // Send a message
    char message[] = "Hi server!";
    if (send(sockfd, message, sizeof(message), 0) == -1) {
        perror("send");
        exit(1);
    }

    close(sockfd);
    shutdown(sockfd, SHUT_RDWR);

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
        runClient();
    }
    
    spdlog::info("Setup complete! Server mode: {}. Listening/sending to port {}", is_server, port);
}