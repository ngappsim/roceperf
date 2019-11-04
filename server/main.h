
#ifndef __SERVER_COMMON_H__
#define __SERVER_COMMON_H__

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sched.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#define TEST_NZ(x) do { if ( (x)) die("error: " #x " failed (returned non-zero)." ); } while (0)
#define TEST_Z(x)  do { if (!(x)) die("error: " #x " failed (returned zero/null)."); } while (0)

static inline void die(const char *reason)
{
    fprintf(stderr, "%s\n", reason);
    exit(EXIT_FAILURE);
}

struct rdmasrv_slave_ctrl {
    int start;
    int stop;
};

struct rdmasrv_stat {
    unsigned long tx_transactions;
    unsigned long rx_transactions;
    unsigned long txbytes;
    unsigned long rxbytes;
};

typedef enum {
    PASSIVE_READ,
    PASSIVE_WRITE,
    ACTIVE_READ,
    ACTIVE_WRITE,
} rdmasrv_mode;

typedef struct rdmasrv_listener_property {
    int buffer_size;
    int loop;
    rdmasrv_mode mode;
} rdmasrv_listener_property_t;

extern struct rdmasrv_stat *g_stats;
extern struct rdmasrv_slave_ctrl *g_ctrl;
extern int g_slave_id;

extern void rdmasrv_run(int port, int buffer_size, int num_qp, rdmasrv_mode mode, int loop);

#define RDMASRV_MAX_WORKER 64

#endif /* __SERVER_COMMON_H__ */
