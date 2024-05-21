#!/bin/bash

PROJECT_DIR=$1

# set include directory
include_dir="include"

# find all source files (*.cpp)
src_files=$(find $PROJECT_DIR -type f -name "*.cpp")

echo "source files: $src_files"

# compile all source files together, and output the executable to test_build,
# if there is an error, exit the script with status code 1
g++ -I $include_dir $src_files -o test_build || exit 1

rm test_build
