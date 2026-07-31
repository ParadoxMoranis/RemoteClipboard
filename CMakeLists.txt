cmake_minimum_required(VERSION 3.16)

project(RemoteClipboard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(UNIX AND NOT APPLE)
    set(RC_LINUX_DEFAULT ON)
else()
    set(RC_LINUX_DEFAULT OFF)
endif()

option(RC_BUILD_SERVERS "Build both server variants" ON)
option(RC_BUILD_CLI "Build the Linux Wayland CLI" ${RC_LINUX_DEFAULT})
option(RC_BUILD_WAYLAND_GUI "Build the Linux Wayland GUI" ${RC_LINUX_DEFAULT})
option(RC_BUILD_WINDOWS_GUI "Build the Windows GUI" ${WIN32})
option(RC_BUILD_TESTS "Build automated tests" ON)
option(RC_STATIC_GNU_RUNTIMES "Statically link GNU C++ runtimes on Linux" ON)

find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_library(remote_clipboard_protocol STATIC
    client_common/protocol.cpp
)
target_include_directories(remote_clipboard_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(remote_clipboard_protocol PUBLIC nlohmann_json::nlohmann_json)
add_library(RemoteClipboard::Protocol ALIAS remote_clipboard_protocol)

if(RC_BUILD_SERVERS OR RC_BUILD_TESTS)
    find_package(SQLite3 REQUIRED)
    if(TARGET SQLite3::SQLite3)
        set(RC_SQLITE_TARGET SQLite3::SQLite3)
    else()
        set(RC_SQLITE_TARGET SQLite::SQLite3)
    endif()
    add_library(remote_clipboard_server_common STATIC
        server_common/filetransfer.cpp
        server_common/frameparser.cpp
        server_common/router.cpp
        server_common/serverapp.cpp
        server_common/serverconfig.cpp
        server_common/sqlitestorage.cpp
        server_common/tcpserver.cpp
    )
    target_include_directories(remote_clipboard_server_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(remote_clipboard_server_common
        PUBLIC
            RemoteClipboard::Protocol
            OpenSSL::SSL
            OpenSSL::Crypto
            ${RC_SQLITE_TARGET}
            Threads::Threads
    )
    if(WIN32)
        target_link_libraries(remote_clipboard_server_common PUBLIC ws2_32)
    endif()
    add_library(RemoteClipboard::ServerCommon ALIAS remote_clipboard_server_common)
endif()

function(rc_apply_runtime_linking target_name)
    if(RC_STATIC_GNU_RUNTIMES AND UNIX AND NOT APPLE AND
       CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(${target_name} PRIVATE -static-libstdc++ -static-libgcc)
    endif()
endfunction()

if(RC_BUILD_SERVERS)
    add_executable(remote_clipboard_server_64 RemoteClipboardServer-64MB/server_main.cpp)
    target_link_libraries(remote_clipboard_server_64 PRIVATE RemoteClipboard::ServerCommon)
    set_target_properties(remote_clipboard_server_64 PROPERTIES OUTPUT_NAME remote-clipboard-server-64mb)
    rc_apply_runtime_linking(remote_clipboard_server_64)

    add_executable(remote_clipboard_server_512 RemoteClipboardServer-512MB/server_main.cpp)
    target_link_libraries(remote_clipboard_server_512 PRIVATE RemoteClipboard::ServerCommon)
    set_target_properties(remote_clipboard_server_512 PROPERTIES OUTPUT_NAME remote-clipboard-server-512mb)
    rc_apply_runtime_linking(remote_clipboard_server_512)
endif()

if(RC_BUILD_CLI)
    if(NOT UNIX OR APPLE)
        message(FATAL_ERROR "RC_BUILD_CLI currently requires Linux")
    endif()
    add_executable(remote_clipboard_cli
        RemoteclipboardCliForLinux/clipboard_sync.cpp
        cli_common/config.cpp
        server_common/filetransfer.cpp
    )
    target_include_directories(remote_clipboard_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(remote_clipboard_cli PRIVATE
        RemoteClipboard::Protocol
        OpenSSL::SSL
        OpenSSL::Crypto
    )
    set_target_properties(remote_clipboard_cli PROPERTIES OUTPUT_NAME remote-clipboard-cli)
    rc_apply_runtime_linking(remote_clipboard_cli)
endif()

if(RC_BUILD_WAYLAND_GUI OR RC_BUILD_WINDOWS_GUI)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Core)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Core)

    set(RC_GUI_COMMON_SOURCES
        gui_common/autostartmanager.cpp
        gui_common/autostartmanager.h
        gui_common/clientconfig.cpp
        gui_common/clientconfig.h
        gui_common/settingsdialog.cpp
        gui_common/settingsdialog.h
    )
endif()

if(RC_BUILD_WAYLAND_GUI)
    if(NOT UNIX OR APPLE)
        message(FATAL_ERROR "RC_BUILD_WAYLAND_GUI is only supported on Linux")
    endif()
    add_executable(remote_clipboard_linux_gui
        RemoteClipboard_Linux_wayland/main.cpp
        RemoteClipboard_Linux_wayland/mainwindow.cpp
        RemoteClipboard_Linux_wayland/mainwindow.h
        RemoteClipboard_Linux_wayland/mainwindow.ui
        RemoteClipboard_Linux_wayland/clipboardmonitor.cpp
        RemoteClipboard_Linux_wayland/clipboardmonitor.h
        RemoteClipboard_Linux_wayland/tcpclient.cpp
        RemoteClipboard_Linux_wayland/tcpclient.h
        ${RC_GUI_COMMON_SOURCES}
    )
    target_include_directories(remote_clipboard_linux_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(remote_clipboard_linux_gui PRIVATE
        RemoteClipboard::Protocol
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Core
    )
    set_target_properties(remote_clipboard_linux_gui PROPERTIES OUTPUT_NAME remote-clipboard-gui)
endif()

if(RC_BUILD_WINDOWS_GUI)
    if(NOT WIN32)
        message(FATAL_ERROR "RC_BUILD_WINDOWS_GUI is only supported on Windows")
    endif()
    add_executable(remote_clipboard_windows_gui WIN32
        RemoteClipboardWindows/main.cpp
        RemoteClipboardWindows/mainwindow.cpp
        RemoteClipboardWindows/mainwindow.h
        RemoteClipboardWindows/mainwindow.ui
        RemoteClipboardWindows/clipboardmonitor.cpp
        RemoteClipboardWindows/clipboardmonitor.h
        RemoteClipboardWindows/tcpclient.cpp
        RemoteClipboardWindows/tcpclient.h
        ${RC_GUI_COMMON_SOURCES}
    )
    target_include_directories(remote_clipboard_windows_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(remote_clipboard_windows_gui PRIVATE
        RemoteClipboard::Protocol
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Core
    )
    set_target_properties(remote_clipboard_windows_gui PROPERTIES OUTPUT_NAME remote-clipboard-windows)
endif()

if(RC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)
if(RC_BUILD_SERVERS)
    install(TARGETS remote_clipboard_server_64 remote_clipboard_server_512
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Server)
    install(FILES README.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/server
        COMPONENT Server)
endif()
if(RC_BUILD_CLI)
    install(TARGETS remote_clipboard_cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Client)
endif()
if(RC_BUILD_WAYLAND_GUI)
    install(TARGETS remote_clipboard_linux_gui
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Client)
endif()
if(RC_BUILD_WINDOWS_GUI)
    install(TARGETS remote_clipboard_windows_gui
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Client)
endif()
if(RC_BUILD_CLI OR RC_BUILD_WAYLAND_GUI OR RC_BUILD_WINDOWS_GUI)
    install(FILES README.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR}/client
        COMPONENT Client)
endif()

set(CPACK_PACKAGE_NAME "remote-clipboard")
set(CPACK_PACKAGE_VENDOR "Remote Clipboard contributors")
set(CPACK_PACKAGE_CONTACT "ParadoxMoranis")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Secure cross-device clipboard synchronization")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/ParadoxMoranis/RemoteClipboard")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_COMPONENTS_ALL Server Client)
set(CPACK_COMPONENT_SERVER_DISPLAY_NAME "Remote Clipboard server")
set(CPACK_COMPONENT_SERVER_DESCRIPTION "64 MiB and 512 MiB Remote Clipboard servers")
set(CPACK_COMPONENT_CLIENT_DISPLAY_NAME "Remote Clipboard client")
set(CPACK_COMPONENT_CLIENT_DESCRIPTION "Linux CLI and Qt Wayland client")

set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "ParadoxMoranis")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_SERVER_PACKAGE_NAME "remote-clipboard-server")
set(CPACK_DEBIAN_CLIENT_PACKAGE_NAME "remote-clipboard-client")
set(CPACK_DEBIAN_CLIENT_PACKAGE_DEPENDS "wl-clipboard")

set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_RPM_PACKAGE_LICENSE "LicenseRef-Unspecified")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Internet")
set(CPACK_RPM_SERVER_PACKAGE_NAME "remote-clipboard-server")
set(CPACK_RPM_CLIENT_PACKAGE_NAME "remote-clipboard-client")
set(CPACK_RPM_CLIENT_PACKAGE_REQUIRES "wl-clipboard")

include(CPack)
