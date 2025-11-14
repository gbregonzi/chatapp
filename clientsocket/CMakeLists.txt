cmake_minimum_required(VERSION 3.10)
project(clientSocket VERSION 1.0 LANGUAGES C CXX) 

include(CTest)
enable_testing()


# Explicitly list all source files
set(SOURCES
    main.cpp
    clientSocket.cpp
    ../utils/logger.cpp
)

# List headers separately (optional, for IDE visibility)
set(HEADERS
    clientSocket.h
    ../utils/logger.h
)

add_executable(clientSocket ${SOURCES} ${HEADERS})
set_property(TARGET clientSocket PROPERTY CMAKE_CXX_STANDARD 20)

if(WIN32)
    # Link Winsock library on Windows
    target_link_libraries(clientSocket ws2_32)
else()
    # Link pthread library on Unix-like systems
    target_link_libraries(clientSocket pthread) 
endif()
