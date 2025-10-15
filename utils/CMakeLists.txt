cmake_minimum_required(VERSION 3.10)
project(outputStream VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
enable_testing()

# Add all .cpp files in the current directory
file(GLOB SOURCES "*.cpp")

add_library(functionWrapper STATIC functionWrapper.cpp functionWrapper.h)
add_library(threadPool STATIC threadPool.h threadPool.cpp functionWrapper.cpp functionWrapper.h)

target_include_directories(functionWrapper PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(threadPool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Optionally, add tests here if needed

