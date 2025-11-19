cmake_minimum_required(VERSION 3.10)
project(outputStream VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
enable_testing()

# Explicitly list all source files
set(SOURCES
    functionWrapper.cpp
    threadPool.cpp
    logger.cpp
)

# List headers separately (optional, for IDE visibility)
set(HEADERS
    functionWrapper.h
    threadSafeQueue.h
    threadPool.h
    logger.h
)

add_library(functionWrapper STATIC ${SOURCES} ${HEADERS})
add_library(threadPool STATIC ${SOURCES} ${HEADERS})
add_library(logger STATIC ${SOURCES} ${HEADERS})

#target_include_directories(functionWrapper PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
#target_include_directories(threadPool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
#target_include_directories(logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})



