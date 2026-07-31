cmake_minimum_required(VERSION 3.10)
project(clipboard_sync)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(clipboard_sync
    clipboard_sync.cpp
    ../cli_common/config.cpp
    ../client_common/protocol.cpp
    ../server_common/filetransfer.cpp
)
target_include_directories(clipboard_sync PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(clipboard_sync PRIVATE
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(UNIX AND NOT APPLE)
    target_link_options(clipboard_sync PRIVATE -static-libstdc++ -static-libgcc)
endif()
