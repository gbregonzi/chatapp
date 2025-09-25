cmake_minimum_required(VERSION 3.15)
project(ChatApp)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# add_library(outputStream utils/outputStream.h)

# Add subdirectories
add_subdirectory(chatserver)
add_subdirectory(clientsocket)