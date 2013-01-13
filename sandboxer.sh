#!/usr/bin/env bash

rm -rf sandbox
mkdir sandbox
cp lind/* sandbox/
cp repylib/* sandbox/
cp posix_call_definition.py sandbox/
cp smart_shim_proxy.py sandbox/
cd sandbox
./smart_shim_proxy.py
