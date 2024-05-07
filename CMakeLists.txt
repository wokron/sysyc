cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

project(sysyc)

# sysyc library
set(LIBRARY_NAME lib${PROJECT_NAME})
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(${LIBRARY_NAME} ${SOURCES})
target_include_directories(${LIBRARY_NAME} PUBLIC include)
# lib and exe with same name
set_property(TARGET ${LIBRARY_NAME} PROPERTY OUTPUT_NAME ${PROJECT_NAME})

# unit tests
enable_testing()
add_subdirectory(tests)

# sysyc compiler
add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBRARY_NAME})
