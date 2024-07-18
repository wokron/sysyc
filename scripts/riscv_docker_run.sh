#!/bin/bash

# this script will:
# 1. build the docker image for the environment of riscv cross compiler
# 2. run the docker container
# 3. use the script riscv_run.sh to compile and run the assembly file in the container

# Arguments:
# $1: the assembly file to compile

# check if the assembly file exists
if [ ! -f $1 ]; then
    echo "Error: the assembly file $1 does not exist"
    exit 1
fi

asm_file=$1

# check if the docker is installed
if ! command -v docker &> /dev/null; then
    echo "Error: docker is not installed"
    exit 1
fi

CONTAINER_NAME="riscv-cross-compiler"
IMAGE_NAME="riscv-cross-compiler"

# check if the container is already running, if not, start the container
if [ ! "$(docker ps -q -f name=$CONTAINER_NAME)" ]; then
    echo "Container $CONTAINER_NAME is not running, starting the container..."

    # check if the image exists, if not, build the image
    if [ ! "$(docker images -q $IMAGE_NAME)" ]; then
        echo "Image $IMAGE_NAME does not exist, building the image..."

        # build the docker image
        docker build -t $IMAGE_NAME -f Dockerfile.riscv-cross-compiler .

        echo "Image $IMAGE_NAME built"
    fi

    # run the container
    docker run -itd --name $CONTAINER_NAME $IMAGE_NAME

    echo "Container $CONTAINER_NAME started"
fi

# copy the assembly file to the container
docker cp $asm_file $CONTAINER_NAME:/root

# run the script riscv_run.sh in the container
docker exec -i $CONTAINER_NAME /bin/bash /root/riscv_run.sh /root/$(basename -- "$asm_file")

# store the exit code of the script
exit_code=$?

# remove the assembly file from the container
docker exec -i $CONTAINER_NAME rm /root/$(basename -- "$asm_file")

# exit with the exit code of the script
exit $exit_code
