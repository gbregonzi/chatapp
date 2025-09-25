cmake_minimum_required(VERSION 3.10)
project(chatserver VERSION 1.0 LANGUAGES C CXX) 

include(CTest)
enable_testing()

# Add all .cpp files in the current directory
file(GLOB SOURCES "*.cpp")

add_executable(chatserver ${SOURCES})

set_property(TARGET chatserver PROPERTY CMAKE_CXX_STANDARD 20)


if(WIN32)
    # Link Winsock library on Windows
    target_link_libraries(chatserver ws2_32)
else()
    # Link pthread library on Unix-like systems 
    target_link_libraries(chatserver pthread)   
endif()
