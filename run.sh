#!/bin/bash

# check if need to export path
if [[ ":$PATH:" != *":$HOME/opt/cross/bin:"* ]]; then
    export PATH="$HOME/opt/cross/bin:$PATH"
fi

# rebuild the project
make clean; make

# run the project
qemu-system-i386 -cdrom myos.iso