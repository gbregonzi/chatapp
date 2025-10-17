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
# Include headers from utils
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../utils)

# Explicitly list all source files
set(SOURCES
    main.cpp
    startServer.cpp
    ../utils/threadPool.cpp
    ../utils/functionWrapper.cpp
    ../utils/logger.cpp
)

# List headers separately (optional, for IDE visibility)
set(HEADERS
    startServer.h
    ../utils/threadPool.h
    ../utils/outputStream.h
    ../utils/functionWrapper.h
    ../utils/logger.h
)
target_sources(chatServer PRIVATE ${SOURCES} ${HEADERS})

# Watch for file changes (optional)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

if(WIN32)
    # Link Winsock library on Windows
    target_link_libraries(chatServer ws2_32)
else()
    # Link pthread library on Unix-like systems 
    target_link_libraries(chatServer pthread)   
endif()