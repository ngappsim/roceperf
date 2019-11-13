# roceperf
sudo apt install libibverbs-dev
sudo apt install rdma-core
sudo apt install rdma-core-dev
sudo apt install librdmacm
sudo apt install librdmacm-dev
sudo apt install ibverbs-utils

ifconfig eth1 192.168.1.{1|2} up
sudo rxe_cfg start
sudo rxe_cfg add eth1

./server -p 2002 -b 8192 -q 1 -w 1 -s 0 -i 20 -l 0
./client -p 2002 -h 192.168.1.2 -b 8192 -q 1 -w 1 -s 2 -i 20 -d 10 -l 0
