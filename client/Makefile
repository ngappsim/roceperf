.PHONY: clean

CFLAGS  := -Wall -Werror -g
LD	  := gcc
LDLIBS  := ${LDLIBS} -lrdmacm -libverbs -lpthread

all: client

client: main.o rdma.o event.o
	${LD} -o $@ $^ ${LDLIBS}

clean:
	rm -f *.o client
