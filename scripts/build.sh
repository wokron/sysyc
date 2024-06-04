#!/bin/bash

# this script is used for building the compiler

# if build directory does not exist, create it
if [ ! -d "build" ]; then
    mkdir build
fi

# enter build directory
cd build

# run cmake
cmake ..

# build the project
make

# exit build directory
cd ..
