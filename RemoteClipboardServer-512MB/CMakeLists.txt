cmake_minimum_required(VERSION 3.10)

project(RemoteClipboardServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加源文件
set(SOURCES
    server_main.cpp
    tcpserver.cpp
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 如果在Windows平台，需要链接wsock32和ws2_32库
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE wsock32 ws2_32)
endif()

include_directories(${PROJECT_SOURCE_DIR}/include) 