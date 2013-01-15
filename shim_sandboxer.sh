#!/usr/bin/env bash

# TODO: watch for file changes and autoreload

rm -rf shim_sandbox
mkdir shim_sandbox
cp lind/* shim_sandbox/
cp repylib/* shim_sandbox/
cp posix_call_definition.py shim_sandbox/
cp smart_shim_proxy.py shim_sandbox/
cd shim_sandbox
./smart_shim_proxy.py
