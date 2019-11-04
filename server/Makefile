.PHONY: clean

CFLAGS  := -Wall -Werror -g
LD	  := gcc
LDLIBS  := ${LDLIBS} -lrdmacm -libverbs -lpthread

all: server

server: main.o rdma.o event.o
	${LD} -o $@ $^ ${LDLIBS}

clean:
	rm -f *.o server
