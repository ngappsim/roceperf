
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
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define TEST_NZ(x) do { if ( (x)) die("error: " #x " failed (returned non-zero)." ); } while (0)
#define TEST_Z(x)  do { if (!(x)) die("error: " #x " failed (returned zero/null)."); } while (0)

static inline void die(const char *reason)
{
    fprintf(stderr, "%s\n", reason);
    exit(EXIT_FAILURE);
}

struct rdmacli_slave_ctrl {
    int start;
    int stop;
};

struct rdmacli_stat {
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
} rdmacli_mode;

#define RDMACLI_MAX_WORKER  64
#define RDMACLI_MAX_IP_PER_WORKER   64
#define RDMACLI_MAX_IP_STR_LEN      16

struct rdmacli_conf {
    int port;
    int buffer_size;
    int num_qp;
//    struct {
//        int num_qp;
//    } qp[RDMACLI_MAX_WORKER];
    int num_worker;
    rdmacli_mode mode;
    int stat_interval;
    int duration;
    int loop;
    char host[RDMACLI_MAX_IP_STR_LEN];
    char client_start[RDMACLI_MAX_IP_STR_LEN];
    int num_ips;
    struct {
        char ip[RDMACLI_MAX_IP_STR_LEN];
        int available;
    } ipmap[RDMACLI_MAX_WORKER];
};

extern struct rdmacli_stat *g_stats;
extern struct rdmacli_slave_ctrl *g_ctrl;
extern struct rdmacli_conf *g_conf;
extern int g_slave_id;

extern void rdmacli_run(void);

#define RDMASRV_MAX_WORKER 64

#endif /* __SERVER_COMMON_H__ */
