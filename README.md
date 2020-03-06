# roceperf

On Ubuntu
=========

1. Install required packages.

sudo apt install libibverbs-dev
sudo apt install rdma-core
sudo apt install rdma-core-dev
sudo apt install librdmacm
sudo apt install librdmacm-dev
sudo apt install ibverbs-utils

2. Setup server and run.
ifconfig eth1 67.67.0.101 up
sudo rxe_cfg start
sudo rxe_cfg add eth1
./server -p 5555 -b 65536 -q 1 -w 12 -s 1 -i 10 -r 8

3. Setup client and run.
ifconfig eth1 67.67.0.1 up
sudo rxe_cfg start
sudo rxe_cfg add eth1
./client -p 5555 -h 67.67.0.101 -b 65536 -q 1 -w 12 -s 3 -i 10 -d 60000 -l 0 -c 67.67.0.1 -n 1 -r 8

On Ixia cloudstorm
==================

Pre-requisites: IXIA roce package (p4 path: //packages/3rdParty/roce/main) compiled locally (e.g. /home/rabhunia/p4/packages/3rdParty/roce/main).

1. Build package.

# ./clean.sh
# ./build.sh /opt/cegl-3.2/mips64-octeon-linux-gnu/gcc-4.7.0-glibc-2.16.0/bin/mips64-octeon-linux-gnu-gcc ~/p4/packages/3rdParty/roce/main

2. Copy executables from ${PWD}/.bin/ to port (e.g. /shared/port/roce/).

# sshpass -p 1x14c0m scp .bin/* root@10.36.83.115:/rw/ports/1/1/roce/
# sshpass -p 1x14c0m scp .bin/* root@10.36.83.115:/rw/ports/1/2/roce/

3. Copy libraries (e.g. /home/rabhunia/p4/packages/3rdParty/roce/main/out/bin/octeon2/8.50/host) and modules (e.g. /home/rabhunia/p4/packages/3rdParty/roce/main/out/bin/octeon2/8.50/linux_3.10) from IXIA roce build to port (e.g. /shared/port/roce/lib and /shared/port/roce/mod).

# cd ~/p4/packages/3rdParty/roce/main/
# sshpass -p 1x14c0m scp out/bin/octeon2/8.50/host/* root@10.36.83.115:/rw/ports/1/1/roce/lib/
# sshpass -p 1x14c0m scp out/bin/octeon2/8.50/host/* root@10.36.83.115:/rw/ports/1/2/roce/lib/
# sshpass -p 1x14c0m scp out/bin/octeon2/8.50/linux_3.10/* root@10.36.83.115:/rw/ports/1/1/roce/mod/
# sshpass -p 1x14c0m scp out/bin/octeon2/8.50/linux_3.10/* root@10.36.83.115:/rw/ports/1/2/roce/mod/

4. Create a IxLoad config in LINUX STACK so that after apply config required test interfaces are created. E.g. One IP (67.67.0.1) in client and one IP (67.67.0.101) in server. Then Apply Config. [OPTIONAL]

5. Prepare server and run.

# sh /shared/port/roce/setup.sh 1 /shared/port/roce/mod /shared/port/roce/lib ixint
# LD_LIBRARY_PATH=/shared/port/roce/lib /shared/port/roce/server -p 5555 -b 65536 -q 1 -w 48 -s 1 -i 10 -r 8

6. Prepare client and run.

# sh /shared/port/roce/setup.sh 1 /shared/port/roce/mod /shared/port/roce/lib ixint
# LD_LIBRARY_PATH=/shared/port/roce/lib /shared/port/roce/client -p 5555 -h 67.67.0.101 -b 65536 -q 1 -w 48 -s 3 -i 10 -d 60000 -l 0 -c 67.67.0.1 -n 1 -r 8

