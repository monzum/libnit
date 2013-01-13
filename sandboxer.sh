#!/bin/bash

rm -rf sandbox
rmdir sandbox
mkdir sandbox
cp lind/* sandbox/
cp repylib/* sandbox/
cp smart_shim_proxy.py sandbox/
cp posix_call_definition.py sandbox/
