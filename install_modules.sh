#!/bin/sh
set -x
# WARNING: this script doesn't check for errors, so you have to enhance it in case any of the commands below fail.
# lsmod
umount /mnt/stbfs
cd /usr/src/hw2-mrai/CSE-506
make
cd /usr/src/hw2-mrai/fs/stbfs
make
rmmod stbfs
insmod stbfs.ko
lsmod
cd /usr/src/
mkdir -p parent
cd parent
mkdir -p .stb
cd /usr/src/
#mount -t stbfs parent/ /mnt/stbfs
mount -t stbfs -o enc=MySecretPa55 parent/ /mnt/stbfs