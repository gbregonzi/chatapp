cmake_minimum_required(VERSION 3.10)
project(outputStream VERSION 1.0 LANGUAGES C CXX) 

include(CTest)
enable_testing()

# Add all .cpp files in the current directory
file(GLOB SOURCES "*.cpp")

set_property(TARGET outputStream PROPERTY CMAKE_CXX_STANDARD 20)

