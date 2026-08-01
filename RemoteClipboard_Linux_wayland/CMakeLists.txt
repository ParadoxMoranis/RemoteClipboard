cmake_minimum_required(VERSION 3.5)

project(RemotePostboard VERSION 0.2.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../gui_common)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS 
    Widgets
    Network
    Core
)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
    Widgets
    Network
    Core
)
find_package(nlohmann_json 3.2.0 REQUIRED)

add_library(remote_clipboard_protocol STATIC
    ../client_common/protocol.cpp
)
target_include_directories(remote_clipboard_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(remote_clipboard_protocol PUBLIC nlohmann_json::nlohmann_json)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    ../gui_common/mainwindow.ui
    ../gui_common/clientconfig.cpp
    ../gui_common/clientconfig.h
    ../gui_common/settingsdialog.cpp
    ../gui_common/settingsdialog.h
    ../gui_common/uilanguage.cpp
    ../gui_common/uilanguage.h
    ../gui_common/uitheme.cpp
    ../gui_common/uitheme.h
    ../gui_common/autostartmanager.cpp
    ../gui_common/autostartmanager.h
    clipboardmonitor.h
    clipboardmonitor.cpp
    tcpclient.h
    tcpclient.cpp
)

add_executable(RemotePostboard
    ${PROJECT_SOURCES}
)

target_link_libraries(RemotePostboard PRIVATE
    remote_clipboard_protocol
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Core
)

# 添加 Qt 头文件路径
target_include_directories(RemotePostboard PRIVATE
    ${Qt${QT_VERSION_MAJOR}Widgets_INCLUDE_DIRS}
    ${Qt${QT_VERSION_MAJOR}Network_INCLUDE_DIRS}
    ${Qt${QT_VERSION_MAJOR}Core_INCLUDE_DIRS}
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.RemotePostboard)
endif()
set_target_properties(RemotePostboard PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS RemotePostboard
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(RemotePostboard)
endif()
