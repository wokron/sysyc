#!/bin/bash

# load the configuration file
source .env

# if REMOTE_REPO is undefined, exit the script
if [ -z $REMOTE_REPO ]; then
    echo "error: REMOTE_REPO is not defined"
    exit 1
fi

# if there isn't a `.deploy` directory, create one
deploy_dir=".deploy"
if [ ! -d $deploy_dir ]; then
    mkdir $deploy_dir
fi

# if the`.deploy` directory is not a git repository, clone the remote repository
if [ ! -d $deploy_dir/.git ]; then
    git clone $REMOTE_REPO $deploy_dir
fi

# pull the latest changes from the remote repository
cd $deploy_dir
git pull
cd ..

# copy the files to the `.deploy` directory
cp -r src $deploy_dir
cp -r include $deploy_dir
cp main.cpp $deploy_dir

# test if the source files could be compiled successfully using deploy_test_build.sh script
if ! bash ./scripts/deploy_test_build.sh $deploy_dir; then
    echo "error: failed to compile the source files"
    exit 1
fi

# commit the changes, the commit message is formated like "Update at 2024-01-01 00:00:00"
cd $deploy_dir
git add .
git commit -m "Update at $(date +'%Y-%m-%d %H:%M:%S')"

# finally, push the changes to the remote repository, username and password are required
git push -u --all $REMOTE_REPO
