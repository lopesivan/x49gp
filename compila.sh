#!/usr/bin/env bash

(   cd qemu/qemu-git; \
    chmod +x configure-small; \
    ./configure-small --extra-cflags=-DX49GP; \
    make -f Makefile-small
)

make

chmod +x newconfig
chmod +x mount-sdcard.sh umount-sdcard.sh
