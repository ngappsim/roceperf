mkdir /dev/infiniband

mknod /dev/infiniband/rdma_cm c 10 254
mknod /dev/infiniband/issm0 c 231 128
mknod /dev/infiniband/ucm0 c 231 384
mknod /dev/infiniband/umad0 c 231 0
mknod /dev/infiniband/uverbs0 c 231 256

cd /shared/port/roce/linux_3.10
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

echo ixint1 > /sys/module/rdma_rxe/parameters/add

echo "export LD_LIBRARY_PATH=.:/shared/port/roce/host"
echo "./server -p 5555 -b 20000 -q 1 -w 8 -s 0 -i 20"
