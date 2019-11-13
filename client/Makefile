.PHONY: clean

CC	:= /opt/cegl-3.2/mips64-octeon-linux-gnu/gcc-4.7.0-glibc-2.16.0/bin/mips64-octeon-linux-gnu-gcc
CFLAGS  := -Wall -Werror -g -I/home/rabhunia/p4/packages/3rdParty/roce/main/out/api/roce/include
LD	  := /opt/cegl-3.2/mips64-octeon-linux-gnu/gcc-4.7.0-glibc-2.16.0/bin/mips64-octeon-linux-gnu-gcc
LDLIBS  += -L/home/rabhunia/p4/packages/3rdParty/roce/main/out/bin/octeon2/8.50/host
LDLIBS  := ${LDLIBS} -lnl-3 -lnl-route-3 -libverbs -lrdmacm -lrxe-rdmav25 -lpthread

all: client

client: main.o rdma.o event.o
	${LD} -o $@ $^ ${LDLIBS}

clean:
	rm -f *.o client
