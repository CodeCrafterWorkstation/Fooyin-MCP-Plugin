cmake_minimum_required(VERSION 3.20)
project(fooyin_mcp_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# WICHTIG: Qt Automoc für Q_OBJECT aktivieren!
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(FOOYIN_SRC_DIR "X:/fooyin")
set(FOOYIN_BUILD_DIR "${FOOYIN_SRC_DIR}/build")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network WebSockets Sql)

add_library(fooyin_mcp_plugin SHARED
    fooyin_wrapper.cpp
    fooyin_wrapper.h
    mcp_plugin.cpp
    mcp_plugin.h
    mcp_server.cpp
    mcp_server.h
    mcp_tools.cpp
    mcp_tools.h
    mcp_logging.cpp
    mcp_logging.h
)

target_include_directories(fooyin_mcp_plugin PRIVATE
    ${FOOYIN_SRC_DIR}/include
    ${FOOYIN_SRC_DIR}/src
    ${FOOYIN_SRC_DIR}/src/core
    ${FOOYIN_SRC_DIR}/src/gui
    ${FOOYIN_BUILD_DIR}/src/core
    ${FOOYIN_BUILD_DIR}/src/gui
    ${FOOYIN_BUILD_DIR}/src/utils
)

target_link_libraries(fooyin_mcp_plugin PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::WebSockets
    Qt6::Sql
    "${FOOYIN_BUILD_DIR}/src/core/Release/fooyin_core.lib"
    "${FOOYIN_BUILD_DIR}/src/gui/Release/fooyin_gui.lib"
)

target_compile_definitions(fooyin_mcp_plugin PRIVATE FOOYIN_IS_DLL)