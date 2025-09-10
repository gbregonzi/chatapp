cmake_minimum_required(VERSION 3.10)
project(multThreadServer VERSION 1.0 LANGUAGES C CXX) 

include(CTest)
enable_testing()

# Add all .cpp files in the current directory
file(GLOB SOURCES "*.cpp")

add_executable(multThreadServer ${SOURCES})

set_property(TARGET multThreadServer PROPERTY CMAKE_CXX_STANDARD 17)

# Link with pthread if needed (for sockets)
find_package(Threads REQUIRED)
target_link_libraries(multThreadServer PRIVATE Threads::Threads)