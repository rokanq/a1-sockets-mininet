cmake_minimum_required(VERSION 3.15)

add_executable(iPerfer iPerfer.cpp)
target_link_libraries(iPerfer spdlog::spdlog cxxopts::cxxopts)