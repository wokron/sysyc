cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

project(sysyc)

# sysyc library
aux_source_directory(src SOURCES)
add_library(libsysyc ${SOURCES})
target_include_directories(libsysyc PUBLIC include)

# unit tests
enable_testing()
add_subdirectory(tests)

# sysyc compiler
add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE libsysyc)
