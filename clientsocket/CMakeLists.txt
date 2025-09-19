cmake_minimum_required(VERSION 3.10)
project(clientsocket VERSION 1.0 LANGUAGES C CXX) 

include(CTest)
enable_testing()

# Add all .cpp files in the current directory
file(GLOB SOURCES "*.cpp")

add_executable(clientsocket ${SOURCES})
set_property(TARGET clientsocket PROPERTY CMAKE_CXX_STANDARD 20)
include_directories(../utils)

if(WIN32)
    # Link Winsock library on Windows
    target_link_libraries(clientsocket ws2_32 outputStream)
else()
    # Link pthread library on Unix-like systems
    target_link_libraries(clientsocket pthread outputStream) 
endif()
