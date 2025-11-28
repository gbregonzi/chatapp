cmake_minimum_required(VERSION 3.15)
project(ChatApp)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add subdirectories
add_subdirectory(chatserver)
if(WIN32)
    add_subdirectory(iocpTest)
else()
    add_subdirectory(epolTest)
endif()
add_subdirectory(clientsocket)
add_subdirectory(utils)