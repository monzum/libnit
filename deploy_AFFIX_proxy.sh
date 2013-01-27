#!/bin/bash

deploy_dir=$1

# Make sure that the deployment directory is provided.
if [ -z "$deploy_dir" ]; then
    echo -e "Usage:\n$ ./deploy_libnit.sh DEPLOY_DIR"
    exit
fi

# Check to see if directory exists. If directory does not exist
# then create it.
if [ ! -d "$deploy_dir" ]; then
    echo -e "Directory '$deploy_dir' does not exist."
    echo -e "Creating directory: $deploy_dir\n"
    mkdir $deploy_dir
else
    echo -e "Deploying AFFIX proxy in directory: $deploy_dir\n"
fi

# Copy over all the required files.
cp -r lind/* $deploy_dir
cp -r repylib/* $deploy_dir
cp smart_shim_proxy.py $deploy_dir
cp posix_call_definition.py $deploy_dir


