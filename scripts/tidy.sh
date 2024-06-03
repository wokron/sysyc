#!/bin/bash

opt_fix=""

# get options from arguments
while getopts "f" opt; do
    case ${opt} in
        f )
            opt_fix="--fix"
            ;;
        \? )
            echo "invalid option: -$OPTARG" 1>&2
            exit 1
            ;;
    esac
done

source_files=`find src -name '*.cpp' -o -name '*.h' \
    | grep 'scanner.*\|parser.*' -v`

clang-tidy ${source_files} \
    ${opt_fix} \
    -p ./build/compile_commands.json \
    -header-filter='.*' \
    -extra-arg=-std=c++17
