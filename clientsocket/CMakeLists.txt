cmake_minimum_required(VERSION 3.10)
project(clientSocket VERSION 1.0 LANGUAGES C CXX) 

include(CTest)
enable_testing()

# Add all .cpp files in the current directory
file(GLOB SOURCES "*.cpp")

add_executable(clientSocket ${SOURCES})

set_property(TARGET clientSocket PROPERTY CMAKE_CXX_STANDARD 17)

# Link with pthread if needed (for sockets)
find_package(Threads REQUIRED)
target_link_libraries(clientSocket PRIVATE Threads::Threads)