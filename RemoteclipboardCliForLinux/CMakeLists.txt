cmake_minimum_required(VERSION 3.10)
project(clipboard_sync)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(nlohmann_json 3.2.0 REQUIRED)

add_executable(clipboard_sync clipboard_sync.cpp)
target_link_libraries(clipboard_sync PRIVATE nlohmann_json::nlohmann_json pthread) 