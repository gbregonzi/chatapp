cmake_minimum_required(VERSION 3.15)
project(ChatApp)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set(CMAKE_PREFIX_PATH "../../../vcpkg/installed/x64-windows/share/fmt")
#find_package(fmt CONFIG REQUIRED)

# Add subdirectories
add_subdirectory(chatserver)
add_subdirectory(iocpTest)
add_subdirectory(clientsocket)
add_subdirectory(utils)