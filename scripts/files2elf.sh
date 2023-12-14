#!/usr/bin/env sh

tmp=$(mktemp) \
&& littlefs_create -b 4096 -c $1 -i $tmp -s $2 \
&& arm-none-eabi-objcopy -I binary -O elf32-littlearm --rename-section .data=.fs_data $tmp $3
