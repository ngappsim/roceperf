

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

#mknod /dev/infiniband/issm0 c 231 64
#mknod /dev/infiniband/issm1 c 231 65
#mknod /dev/infiniband/issm2 c 231 66
#mknod /dev/infiniband/issm3 c 231 67
#mknod /dev/infiniband/issm4 c 231 68
#mknod /dev/infiniband/issm5 c 231 69
#mknod /dev/infiniband/issm6 c 231 70
#mknod /dev/infiniband/issm7 c 231 71
#mknod /dev/infiniband/issm8 c 231 72
#mknod /dev/infiniband/issm9 c 231 73
#mknod /dev/infiniband/issm10 c 231 74
#mknod /dev/infiniband/issm11 c 231 75
#mknod /dev/infiniband/issm12 c 231 76
#mknod /dev/infiniband/issm13 c 231 77
#mknod /dev/infiniband/issm14 c 231 78
#mknod /dev/infiniband/issm15 c 231 79
#mknod /dev/infiniband/issm16 c 231 80
#mknod /dev/infiniband/issm17 c 231 81
#mknod /dev/infiniband/issm18 c 231 82
#mknod /dev/infiniband/issm19 c 231 83

#mknod /dev/infiniband/ucm0 c 231 224
#mknod /dev/infiniband/ucm1 c 231 225
#mknod /dev/infiniband/ucm2 c 231 226
#mknod /dev/infiniband/ucm3 c 231 227
#mknod /dev/infiniband/ucm4 c 231 228
#mknod /dev/infiniband/ucm5 c 231 229
#mknod /dev/infiniband/ucm6 c 231 230
#mknod /dev/infiniband/ucm7 c 231 231
#mknod /dev/infiniband/ucm8 c 231 232
#mknod /dev/infiniband/ucm9 c 231 233
#mknod /dev/infiniband/ucm10 c 231 234
#mknod /dev/infiniband/ucm11 c 231 235
#mknod /dev/infiniband/ucm12 c 231 236
#mknod /dev/infiniband/ucm13 c 231 237
#mknod /dev/infiniband/ucm14 c 231 238
#mknod /dev/infiniband/ucm15 c 231 239
#mknod /dev/infiniband/ucm16 c 231 240
#mknod /dev/infiniband/ucm17 c 231 241
#mknod /dev/infiniband/ucm18 c 231 242
#mknod /dev/infiniband/ucm19 c 231 243
#
#mknod /dev/infiniband/umad0 c 231 0
#mknod /dev/infiniband/umad1 c 231 1
#mknod /dev/infiniband/umad2 c 231 2
#mknod /dev/infiniband/umad3 c 231 3
#mknod /dev/infiniband/umad4 c 231 4
#mknod /dev/infiniband/umad5 c 231 5
##mknod /dev/infiniband/umad6 c 231 6
#mknod /dev/infiniband/umad7 c 231 7
#mknod /dev/infiniband/umad8 c 231 8
#mknod /dev/infiniband/umad9 c 231 9
#mknod /dev/infiniband/umad10 c 231 10
#mknod /dev/infiniband/umad11 c 231 11
#mknod /dev/infiniband/umad12 c 231 12
#mknod /dev/infiniband/umad13 c 231 13
#mknod /dev/infiniband/umad14 c 231 14
#mknod /dev/infiniband/umad15 c 231 15
#mknod /dev/infiniband/umad16 c 231 16
#mknod /dev/infiniband/umad17 c 231 17
#mknod /dev/infiniband/umad18 c 231 18
#mknod /dev/infiniband/umad19 c 231 19
#
#mknod /dev/infiniband/uverbs0 c 231 192
#mknod /dev/infiniband/uverbs1 c 231 193
#mknod /dev/infiniband/uverbs2 c 231 194
#mknod /dev/infiniband/uverbs3 c 231 195
##mknod /dev/infiniband/uverbs4 c 231 196
#mknod /dev/infiniband/uverbs5 c 231 197
#mknod /dev/infiniband/uverbs6 c 231 198
#mknod /dev/infiniband/uverbs7 c 231 199
#mknod /dev/infiniband/uverbs8 c 231 200
#mknod /dev/infiniband/uverbs9 c 231 201
#mknod /dev/infiniband/uverbs10 c 231 202
##mknod /dev/infiniband/uverbs11 c 231 203
#mknod /dev/infiniband/uverbs12 c 231 204
#mknod /dev/infiniband/uverbs13 c 231 205
#mknod /dev/infiniband/uverbs14 c 231 206
#mknod /dev/infiniband/uverbs15 c 231 207
#mknod /dev/infiniband/uverbs16 c 231 208
#mknod /dev/infiniband/uverbs17 c 231 209
#mknod /dev/infiniband/uverbs18 c 231 210
#mknod /dev/infiniband/uverbs19 c 231 211

echo "Inserting modules ..."

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

echo "DONE"

#echo ixint1 > /sys/module/rdma_rxe/parameters/add
#echo ixint2 >> /sys/module/rdma_rxe/parameters/add
#echo ixint3 >> /sys/module/rdma_rxe/parameters/add
#echo ixint4 >> /sys/module/rdma_rxe/parameters/add
#echo ixint5 >> /sys/module/rdma_rxe/parameters/add
#echo ixint6 >> /sys/module/rdma_rxe/parameters/add
#echo ixint7 >> /sys/module/rdma_rxe/parameters/add
#echo ixint8 >> /sys/module/rdma_rxe/parameters/add
#echo ixint9 >> /sys/module/rdma_rxe/parameters/add
#echo ixint10 >> /sys/module/rdma_rxe/parameters/add
#echo ixint11 > /sys/module/rdma_rxe/parameters/add
#echo ixint12 >> /sys/module/rdma_rxe/parameters/add
#echo ixint13 >> /sys/module/rdma_rxe/parameters/add
#echo ixint14 >> /sys/module/rdma_rxe/parameters/add
#echo ixint15 >> /sys/module/rdma_rxe/parameters/add
#echo ixint16 >> /sys/module/rdma_rxe/parameters/add
#echo ixint17 >> /sys/module/rdma_rxe/parameters/add
#echo ixint18 >> /sys/module/rdma_rxe/parameters/add
#echo ixint19 >> /sys/module/rdma_rxe/parameters/add
#echo ixint20 >> /sys/module/rdma_rxe/parameters/add

echo "Adding devices ..."

a=0
while [ $a -lt $numdev ]
do
    if [ $a -eq 0 ]; then
        echo ixint$a > /sys/module/rdma_rxe/parameters/add
    else
        echo ixint$a >> /sys/module/rdma_rxe/parameters/add
    fi
    a=`expr $a + 1`
done

echo "DONE"

echo "export LD_LIBRARY_PATH=.:/shared/port/roce/host"
echo "./client -p 5555 -h 67.67.0.101 -b 20000 -q 1 -w 8 -s 2 -i 20 -d 60 -l 0 -c 67.67.0.1 -n 10"
