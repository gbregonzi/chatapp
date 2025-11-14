cmake_minimum_required(VERSION 3.10)
project(chatServer VERSION 1.0 LANGUAGES C CXX) 

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
enable_testing()

# Add all .cpp files in the current directory
file(GLOB SOURCES "*.cpp" "*.h")

add_library(startServer ${SOURCES})
add_library(handleConnectionsWindows ${SOURCES})

add_executable(chatServer ${SOURCES})

set_property(TARGET chatServer PROPERTY CMAKE_CXX_STANDARD 20)

# Explicitly list all source files
set(SOURCES
    main.cpp
    chatserver.cpp
    startserver.cpp
    handleConnectionsWindows.cpp
    ../utils/logger.cpp
    ../utils/util.cpp
)

# List headers separately (optional, for IDE visibility)
set(HEADERS
    chatserver.h
    startserver.h
    chatServerFactory.h
    handleConnectionsWindows.h
    ../utils/logger.h
    ../utils/util.h
)

# Watch for file changes (optional)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
target_sources(chatServer PRIVATE ${SOURCES} ${HEADERS})

if(WIN32)
    # Link Winsock library on Windows
    target_link_libraries(chatServer ws2_32)
else()
    # Link pthread library on Unix-like systems 
    target_link_libraries(chatServer pthread)   
endif()
