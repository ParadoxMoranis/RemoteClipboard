cmake_minimum_required(VERSION 3.16)

project(RemoteClipboardWindowsClient VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Core)
find_package(nlohmann_json 3.2.0 REQUIRED)

add_library(remote_clipboard_protocol STATIC ../client_common/protocol.cpp)
target_include_directories(remote_clipboard_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(remote_clipboard_protocol PUBLIC nlohmann_json::nlohmann_json)

add_executable(RemoteClipboardWindowsClient WIN32
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    clipboardmonitor.cpp
    clipboardmonitor.h
    tcpclient.cpp
    tcpclient.h
    ../gui_common/clientconfig.cpp
    ../gui_common/clientconfig.h
    ../gui_common/settingsdialog.cpp
    ../gui_common/settingsdialog.h
    ../gui_common/autostartmanager.cpp
    ../gui_common/autostartmanager.h
)
target_include_directories(RemoteClipboardWindowsClient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(RemoteClipboardWindowsClient PRIVATE
    remote_clipboard_protocol
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Core
)

include(GNUInstallDirs)
install(TARGETS RemoteClipboardWindowsClient
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(RemoteClipboardWindowsClient)
endif()
