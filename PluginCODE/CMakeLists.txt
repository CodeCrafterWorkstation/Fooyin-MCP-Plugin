cmake_minimum_required(VERSION 3.19)
project(fooyin_mcp_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Concurrent Widgets)
find_package(fooyin REQUIRED)

add_library(fooyin_mcp_plugin SHARED
    mcp_plugin.cpp
    mcp_plugin.h
    mcp_server.cpp
    mcp_server.h
    fooyin_wrapper.cpp
    fooyin_wrapper.h
)

target_link_libraries(fooyin_mcp_plugin PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Concurrent
    Qt6::Widgets
    fooyin::sdk
)

target_include_directories(fooyin_mcp_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
