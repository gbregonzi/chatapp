cmake_minimum_required(VERSION 3.10)
project(epolTest VERSION 1.0 LANGUAGES C CXX) 

include(CTest)
enable_testing()

# Add all .cpp files in the current directory
set(SOURCES "epol.cpp")

add_executable(epol ${SOURCES})
set_property(TARGET epol PROPERTY CMAKE_CXX_STANDARD 20)

target_link_libraries(epol pthread) 
