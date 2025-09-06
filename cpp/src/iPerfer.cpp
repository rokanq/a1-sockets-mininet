#include <spdlog/spdlog.h>
#include <cxxopts.hpp>

#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int runServer(){
    spdlog::info("We got here");
    return 0;
}

int runClient(){
    spdlog::info("We got here too gang");
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
        runServer();
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