cmake_minimum_required(VERSION 3.19)
project(fooyin_mcp_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Paths to your Fooyin build
set(FOOYIN_SRC_DIR "C:/Users/slu/AndroidStudioProjects/fooyin-src")
set(FOOYIN_BUILD_DIR "${FOOYIN_SRC_DIR}/build")

find_package(Qt6 REQUIRED COMPONENTS Core Network Concurrent Widgets WebSockets Sql)

add_library(fooyin_mcp_plugin SHARED
    mcp_plugin.cpp
    mcp_plugin.h
    mcp_server.cpp
    mcp_server.h
    mcp_tools.cpp
    mcp_tools.h
    fooyin_wrapper.cpp
    fooyin_wrapper.h
    mcp_logging.cpp
    mcp_logging.h
)

# Include directories including subfolders of core/gui
target_include_directories(fooyin_mcp_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FOOYIN_SRC_DIR}/include
    ${FOOYIN_SRC_DIR}/src
    ${FOOYIN_SRC_DIR}/src/core
    ${FOOYIN_SRC_DIR}/src/gui
    ${FOOYIN_BUILD_DIR}/src/core
    ${FOOYIN_BUILD_DIR}/src/gui
    ${FOOYIN_BUILD_DIR}/src/utils
)

# Link against Qt and compiled Fooyin libraries
target_link_libraries(fooyin_mcp_plugin PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Concurrent
    Qt6::Widgets
    Qt6::WebSockets
    Qt6::Sql
    "${FOOYIN_BUILD_DIR}/src/core/Debug/fooyin_core.lib"
    "${FOOYIN_BUILD_DIR}/src/gui/Debug/fooyin_gui.lib"
)

target_compile_definitions(fooyin_mcp_plugin PRIVATE FOOYIN_IS_DLL)