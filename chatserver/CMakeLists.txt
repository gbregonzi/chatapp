cmake_minimum_required(VERSION 3.10)
project(chatServer VERSION 1.0 LANGUAGES C CXX) 

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
enable_testing()


# Explicitly list all source files
set(SOURCES
    main.cpp
    chatserver.cpp
    startserver.cpp
 #   ../utils/logger.cpp
 #   ../utils/util.cpp
)

# List headers separately (optional, for IDE visibility)
set(HEADERS
    chatserver.h
    startserver.h
    chatServerFactory.h
#    ../utils/logger.h
#    ../utils/util.h
)


if(WIN32)
    message(STATUS "Compiling for Windows")
    list(APPEND SOURCES handleConnectionsWindows.cpp)
    list(APPEND HEADERS handleConnectionsWindows.h)
else()    
    message(STATUS "Compiling for Linux")
    set(CMAKE_PREFIX_PATH "../../../vcpkg/installed/x64-windows/share/fmt")
    find_package(fmt CONFIG REQUIRED)
    list(APPEND SOURCES handleConnectionsLinux.cpp ../utils/threadPool.cpp ../utils/functionWrapper.cpp)
    list(APPEND HEADERS handleConnectionsLinux.h ../utils/threadPool.h ../utils/functionWrapper.h)
endif()    


add_library(startServer ${SOURCES})

add_executable(chatServer ${SOURCES})

set_property(TARGET chatServer PROPERTY CMAKE_CXX_STANDARD 20)

target_sources(chatServer PRIVATE ${SOURCES} ${HEADERS})

if(WIN32)
    # Link Winsock library on Windows
    target_link_libraries(chatServer PRIVATE ws2_32 logger util)
else()
    # Link pthread library on Unix-like systems 
    target_link_libraries(chatServer PRIVATE pthread fmt::fmt logger util)   
endif()
