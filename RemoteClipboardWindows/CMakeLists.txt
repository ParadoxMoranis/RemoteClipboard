cmake_minimum_required(VERSION 3.5)

# 设置工具链
set(CMAKE_C_COMPILER "C:/Qt/Tools/mingw1310_64/bin/gcc.exe")
set(CMAKE_CXX_COMPILER "C:/Qt/Tools/mingw1310_64/bin/g++.exe")

project(dmo VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置Qt路径
set(Qt6_DIR "C:/Qt/6.8.1/mingw_64/lib/cmake/Qt6")
set(CMAKE_PREFIX_PATH "C:/Qt/6.8.1/mingw_64")

# 添加运行时路径
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install")
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

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

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    clipboardmonitor.h
    clipboardmonitor.cpp
    tcpclient.h
    tcpclient.cpp
    tcpserver.h
    tcpserver.cpp
)

add_executable(dmo WIN32
    ${PROJECT_SOURCES}
)

target_link_libraries(dmo PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Core
)

# 添加Qt运行时路径
set_target_properties(dmo PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# 复制Qt DLL到目标目录
if(WIN32)
    set(QT_INSTALL_PATH "C:/Qt/6.8.1/mingw_64")
    add_custom_command(TARGET dmo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_INSTALL_PATH}/bin/Qt6Core.dll"
            "${QT_INSTALL_PATH}/bin/Qt6Gui.dll"
            "${QT_INSTALL_PATH}/bin/Qt6Widgets.dll"
            "${QT_INSTALL_PATH}/bin/Qt6Network.dll"
            $<TARGET_FILE_DIR:dmo>
    )
    
    # 复制平台插件
    add_custom_command(TARGET dmo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:dmo>/platforms"
    )
    add_custom_command(TARGET dmo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_INSTALL_PATH}/plugins/platforms/qwindows.dll"
            "$<TARGET_FILE_DIR:dmo>/platforms"
    )
endif()

# 添加 Qt 头文件路径
target_include_directories(dmo PRIVATE
    ${Qt${QT_VERSION_MAJOR}Widgets_INCLUDE_DIRS}
    ${Qt${QT_VERSION_MAJOR}Network_INCLUDE_DIRS}
    ${Qt${QT_VERSION_MAJOR}Core_INCLUDE_DIRS}
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.dmo)
endif()
set_target_properties(dmo PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS dmo
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(dmo)
endif()
