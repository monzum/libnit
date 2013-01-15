#!/usr/bin/env bash

# TODO: watch for file changes and autoreload

# remove previous sandbox and prepare a new one instead
rm -rf test_sandbox
mkdir test_sandbox
cp tests/* test_sandbox/
cp Makefile test_sandbox/
cp libnetworkinterpose.c test_sandbox/
cd test_sandbox

# compile system call interposition library
make
# compile unit tests
# NOTE: incorporate tests into Makefile
gcc test_send.c -o test_send

# interpose selected system calls
shimlib_path=`pwd`/libnetworkinterpose.so
export LD_PRELOAD=$LD_PRELOAD:$shimlib_path

# run unit tests
./test_send
