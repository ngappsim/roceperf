#!/bin/bash

# Builds ixia proprietary kernal modules.

usage () {
    echo "Usage: $0 cpu cross_prefix kernel_src_dir module_directory extra_cflags"
    echo "Example: $0 ppc750 /opt/cegl-2.0/powerpc-750-linux-gnu/gcc-3.3.4-glibc-2.3.3/bin/powerpc-750-linux-gnu- ~/ksk/ixia-linux/Source/ppc750/kernel linux/port/kernel/ixllm -I."
    echo
    echo "Builds the module in the specified module directory."
    exit 1
}

echo $#

[ $# -gt 4 ] || usage

set -x -e

CPU=$1
CROSS=$2
KERNEL_SOURCE=$3

cd $4
shift 4
EXTRA_CFLAGS=$@
MOD_DIR=$(pwd)

if [ -n "$SCONS_JOBS" ]; then
    MAKEFLAGS="-j $SCONS_JOBS"
else
    MAKEFLAGS=""
fi

make $MAKEFLAGS ARCH=$CPU CROSS_COMPILE=$CROSS -C $KERNEL_SOURCE KERNEL_SOURCE=$KERNEL_SOURCE SUBDIRS=$MOD_DIR \
     EXTRA_CFLAGS="$EXTRA_CFLAGS" modules

cp -vf pktgen.ko ../.bin/
