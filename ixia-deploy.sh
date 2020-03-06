#!/bin/bash

# $1 - /opt/cegl-3.2/mips64-octeon-linux-gnu/gcc-4.7.0-glibc-2.16.0/bin/mips64-octeon-linux-gnu-gcc
# $2 - ~/p4/packages/3rdParty/roce/main
# $3 - client chassis IP
# $4 - client card
# $5 - client port
# $6 - server chassis IP
# $7 - server card
# $8 - server port

if [ $# -ne 8 ]; then
    echo "Usage: sh $0 <compiler path> <roce build path> <client chassis IP> <client card> <client port> <server chassis IP> <server card> <server port>"
    echo "Example: sh $0 /opt/cegl-3.2/mips64-octeon-linux-gnu/gcc-4.7.0-glibc-2.16.0/bin/mips64-octeon-linux-gnu-gcc ~/p4/packages/3rdParty/roce/main 10.39.48.116 1 1 10.39.48.116 1 5"
    exit
fi

echo "Cleaning ..."
./clean.sh
echo "Building ..."
./build.sh $1 $2

echo "Copying ..."
sshpass -p 1x14c0m scp .bin/* root@$3:/rw/ports/$4/$5/roce/
sshpass -p 1x14c0m scp .bin/* root@$6:/rw/ports/$7/$8/roce/

cd $2 && sshpass -p 1x14c0m scp out/bin/octeon2/8.50/host/* root@$3:/rw/ports/$4/$5/roce/lib/ && sshpass -p 1x14c0m scp out/bin/octeon2/8.50/host/* root@$6:/rw/ports/$7/$8/roce/lib/ && sshpass -p 1x14c0m scp out/bin/octeon2/8.50/linux_3.10/* root@$3:/rw/ports/$4/$5/roce/mod/ && sshpass -p 1x14c0m scp out/bin/octeon2/8.50/linux_3.10/* root@$6:/rw/ports/$7/$8/roce/mod/

echo "DONE"
echo "Now run setup.sh in both sides. E.g. sh /shared/port/roce/setup.sh 1 /shared/port/roce/mod /shared/port/roce/lib ixint"
