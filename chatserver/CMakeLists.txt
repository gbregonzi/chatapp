cmake_minimum_required(VERSION 3.10)
project(chatServer VERSION 1.0 LANGUAGES C CXX) 

include(CTest)
enable_testing()

# Add all .cpp files in the current directory
file(GLOB SOURCES "*.cpp" "*.h")


add_library(startServer ${SOURCES})
include_directories(../utils)

add_executable(chatServer ${SOURCES})

set_property(TARGET chatServer PROPERTY CMAKE_CXX_STANDARD 20)


if(WIN32)
    # Link Winsock library on Windows
    target_link_libraries(chatServer startServer functionWrapper threadPool ws2_32)
else()
    # Link pthread library on Unix-like systems 
    target_link_libraries(chatServer startServer functionWrapper threadPool pthread)   
endif()
