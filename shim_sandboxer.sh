#!/usr/bin/env bash

# TODO: watch for file changes and autoreload

# remove previous sandbox and prepare a new one instead
rm -rf shim_sandbox
mkdir shim_sandbox
cp lind/* shim_sandbox/
cp repylib/* shim_sandbox/
cp posix_call_definition.py shim_sandbox/
cp smart_shim_proxy.py shim_sandbox/
cd shim_sandbox

# run shim proxy
./smart_shim_proxy.py
