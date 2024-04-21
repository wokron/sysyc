cmake_minimum_required(VERSION 3.10)

set (CMAKE_CXX_STANDARD 17)

project(sysyc)

enable_testing()

add_subdirectory(tests)

add_executable(${PROJECT_NAME}
    main.cpp
)
