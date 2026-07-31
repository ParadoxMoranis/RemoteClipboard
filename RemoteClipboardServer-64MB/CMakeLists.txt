cmake_minimum_required(VERSION 3.16)
project(RemoteClipboardServer64 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)
if(TARGET SQLite3::SQLite3)
    set(RC_SQLITE_TARGET SQLite3::SQLite3)
else()
    set(RC_SQLITE_TARGET SQLite::SQLite3)
endif()

add_executable(RemoteClipboardServer
    server_main.cpp
    ../client_common/protocol.cpp
    ../server_common/filetransfer.cpp
    ../server_common/frameparser.cpp
    ../server_common/router.cpp
    ../server_common/serverapp.cpp
    ../server_common/serverconfig.cpp
    ../server_common/sqlitestorage.cpp
    ../server_common/tcpserver.cpp
)
target_include_directories(RemoteClipboardServer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(RemoteClipboardServer PRIVATE
    nlohmann_json::nlohmann_json OpenSSL::SSL OpenSSL::Crypto ${RC_SQLITE_TARGET} Threads::Threads)
if(UNIX AND NOT APPLE)
    target_link_options(RemoteClipboardServer PRIVATE -static-libstdc++ -static-libgcc)
endif()
if(WIN32)
    target_link_libraries(RemoteClipboardServer PRIVATE ws2_32)
endif()
