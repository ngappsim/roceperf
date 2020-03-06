#!/bin/bash

if [ $# -ne 2 ]; then
    echo "Usage: sh $0 <compiler path> <roce build path>"
    echo "Example: sh $0 /opt/cegl-3.2/mips64-octeon-linux-gnu/gcc-4.7.0-glibc-2.16.0/bin/mips64-octeon-linux-gnu-gcc /home/rabhunia/p4/packages/3rdParty/roce/main"
    exit
fi

IXIA_ROCE_PATH=$2
IXIA_COMPILER=$1

IXIA_ROCE_PATH=$IXIA_ROCE_PATH IXIA_COMPILER=$IXIA_COMPILER make -C client/ all
IXIA_ROCE_PATH=$IXIA_ROCE_PATH IXIA_COMPILER=$IXIA_COMPILER make -C server/ all
IXIA_ROCE_PATH=$IXIA_ROCE_PATH IXIA_COMPILER=$IXIA_COMPILER make -C pingpong/ all

mkdir -p .bin

cp -vf setup.sh .bin/
cp -vf rate.sh .bin/
cp -vf server/server .bin/
cp -vf client/client .bin/
cp -vf pingpong/uc_pingpong .bin/
