#!/bin/bash

# this script is used for building the compiler

profile="build"
flag=""
if [ "$1" == "debug" ]; then
    profile="debug"
    flag="$flag -DCMAKE_BUILD_TYPE=Debug"
fi

# if profile directory does not exist, create it
if [ ! -d $profile ]; then
    mkdir $profile
fi

# enter profile directory
cd $profile

# run cmake
cmake .. $flag

# build the project
make

# exit profile directory
cd ..
