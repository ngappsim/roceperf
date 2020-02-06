#!/bin/bash

if [ $# -ne 4 ]; then
    echo "sh $0 <number of NIC> <module path> <library path> <NIC name>"
    echo "Example: sh $0 1 /shared/port/roce/mod /shared/port/roce/lib ixint"
    exit
fi

mkdir /dev/infiniband

mknod /dev/infiniband/rdma_cm c 10 254

numdev=$1

issm_major=231
ucm_major=231
umad_major=231
uverbs_major=231

echo "Creating issmX, ucmX, umadX, uverbsX ..."

a=0
while [ $a -lt $numdev ]
do
    issm_minor=`expr $a + 128`
    mknod /dev/infiniband/issm$a c 231 $issm_minor
    ucm_minor=`expr $a + 384`
    mknod /dev/infiniband/ucm$a c 231 $ucm_minor
    mknod /dev/infiniband/umad$a c 231 $a
    uverbs_minor=`expr $a + 256`
    mknod /dev/infiniband/uverbs$a c 231 $uverbs_minor
    a=`expr $a + 1`
done

echo "DONE"

echo "Inserting modules ..."

cd $2/
insmod udp_tunnel.ko
insmod ib_core.ko
insmod ib_cm.ko
insmod iw_cm.ko
insmod rdma_cm.ko
insmod ib_umad.ko
insmod ib_uverbs.ko
insmod ib_ucm.ko
insmod rdma_ucm.ko
insmod umem.ko
insmod rdmavt.ko
insmod ib_ipoib.ko
insmod rdma_rxe.ko

echo "DONE"

echo "Adding devices ..."

name=$4

a=0
while [ $a -lt $numdev ]
do
    x=`expr $a + 1`
    echo "Adding $name$x ..."
    if [ $a -eq 0 ]; then
        echo $name$x > /sys/module/rdma_rxe/parameters/add
    else
        echo $name$x >> /sys/module/rdma_rxe/parameters/add
    fi
    a=`expr $a + 1`
done

echo "DONE"

echo "Run in server: LD_LIBRARY_PATH=/shared/port/roce/lib ./shared/port/roce/server -p 5555 -b 65536 -q 1 -w 12 -s 1 -i 10 -r 8"
echo "Run in client: LD_LIBRARY_PATH=/shared/port/roce/lib ./shared/port/roce/client -p 5555 -h 67.67.0.101 -b 65536 -q 1 -w 12 -s 3 -i 10 -d 60000 -l 0 -c 67.67.0.1 -n 1 -r 8"
