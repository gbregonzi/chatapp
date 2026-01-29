cmake_minimum_required(VERSION 3.10)
project(clientSocket VERSION 1.0 LANGUAGES C CXX) 

include(CTest)
enable_testing()


# Explicitly list all source files
set(SOURCES
    main.cpp
    clientSocket.cpp
)

# List headers separately (optional, for IDE visibility)
set(HEADERS
    clientSocket.h
)

if (UNIX)
    set(CMAKE_PREFIX_PATH "../../../vcpkg/installed/x64-windows/share/fmt")
    find_package(fmt CONFIG REQUIRED)
endif()

add_executable(clientSocket ${SOURCES} ${HEADERS})
set_property(TARGET clientSocket PROPERTY CMAKE_CXX_STANDARD 20)

if(WIN32)
    # Link Winsock library on Windows
    message(STATUS "Compiling for Windows")
    target_link_libraries(clientSocket PRIVATE ws2_32 logger util)
else()
    # Link pthread library on Unix-like systems
    message(STATUS "Compiling for Linux")
    target_link_libraries(clientSocket PRIVATE pthread fmt::fmt logger util) 
endif()
