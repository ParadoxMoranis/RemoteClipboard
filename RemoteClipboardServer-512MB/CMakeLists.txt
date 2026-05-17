cmake_minimum_required(VERSION 3.10)

project(RemoteClipboardServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(${PROJECT_NAME}
    server_main.cpp
    ../server_common/tcpserver.cpp
    ../server_common/filetransfer.cpp
    ../server_common/serverconfig.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE wsock32 ws2_32)
endif()
