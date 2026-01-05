cmake_minimum_required(VERSION 3.16)

set(CMAKE_INSTALL_BINARY_DIR bin CACHE STRING "Installation subdirectory for compiled executables and shared libraries.")
set(CMAKE_INSTALL_LIBRARY_DIR lib CACHE STRING "Installation subdirectory for compiled static libraries.")
set(CMAKE_INSTALL_SOURCE_DIR include CACHE STRING "Installation subdirectory for source code.")
set(CMAKE_INSTALL_SCRIPT_DIR script CACHE STRING "Installation subdirectory for scripts.")

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(INSTALL_CONFIG_DIR ${CMAKE_INSTALL_LIBRARY_DIR}/cmake)
set(INSTALL_BINARY_DIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINARY_DIR})
set(INSTALL_SCRIPT_DIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SCRIPT_DIR})

set(CARTOCROW_INSTALL_DIR "${CMAKE_INSTALL_SOURCE_DIR}/cartocrow")

project(cartocrow-modulename
    VERSION 0.0.1
    DESCRIPTION "CartoCrow TemplateModule"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CartoCrow REQUIRED)

# Let CMake automatically handle Qt's "Meta-Object Compiler" preprocessor
set(CMAKE_AUTOMOC ON)

# All source files should use include paths relative to the source root
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(library)
add_subdirectory(frontend)
add_subdirectory(test)



