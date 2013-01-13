#!/usr/bin/env bash

make
source load_shim_proxy.sh
strace -e trace=desc,file,network wget www.google.com
