.PHONY: clean

CC	:= ${IXIA_COMPILER}
CFLAGS  := -Wall -Werror -g -I${IXIA_ROCE_PATH}/out/api/roce/include
LD	  := ${IXIA_COMPILER}
LDLIBS  += -L${IXIA_ROCE_PATH}/out/bin/octeon2/8.50/host
LDLIBS  := ${LDLIBS} -lnl-3 -lnl-route-3 -libverbs -lrdmacm -lrxe-rdmav25 -lpthread

all: uc_pingpong

uc_pingpong: pingpong.o uc_pingpong.o
	${LD} -o $@ $^ ${LDLIBS}

clean:
	rm -f *.o uc_pingpong tags

debug:
