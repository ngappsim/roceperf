
#include "main.h"

struct rdmasrv_stat *g_stats;
struct rdmasrv_slave_ctrl *g_ctrl;

int g_port;
int g_loop;
int g_buffer_size;
int g_num_worker;
int g_send;
int g_stat_interval;
int g_slave_id;
int g_num_qp;

static void signal_handler(int sig)
{
    switch(sig) {
        case SIGCHLD:
            fprintf(stdout, "Child died.\n");
            break;
        case SIGINT:
        case SIGTERM:
            g_ctrl->stop = 1;
            break;
        default:
            break;
    }
}

int main(int argc, char **argv)
{
    pid_t pids[RDMASRV_MAX_WORKER];
    int opt, i;
    unsigned long prev_rx_tr = 0, prev_tx_tr = 0, prev_rx_b = 0, prev_tx_b = 0;

    while((opt = getopt(argc, argv, "p:b:q:w:s:i:l:")) != -1) {
        switch (opt) {
            case 'p':
                g_port = atoi(optarg);
                break;
            case 'b':
                g_buffer_size = atoi(optarg);
                break;
            case 'q':
                g_num_qp = atoi(optarg);
                break;
            case 'w':
                g_num_worker = atoi(optarg);
                break;
            case 's':
                g_send = atoi(optarg);
                break;
            case 'i':
                g_stat_interval = atoi(optarg);
                break;
            case 'l':
                g_loop = atoi(optarg);
                break;
            default:
                fprintf(stderr, "server -p <start port> -b <buffer size> -q <number of queue pairs per connections> -w <number of workers> -s <0: passive read, 1: passive write, 2: active read, 3: active write> -i <stat interval>\n");
                exit(-1);
        }
    }

    signal(SIGCHLD, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGKILL, signal_handler);

    g_ctrl = mmap(NULL, sizeof(struct rdmasrv_slave_ctrl), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (g_ctrl == NULL) {
        fprintf(stderr, "Failed to allocate slave ctrl message.\n");
        exit(-2);
    }

    g_stats = mmap(NULL, sizeof(struct rdmasrv_stat), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (g_stats == NULL) {
        fprintf(stderr, "Failed to allocate stats.\n");
        exit(-3);
    }

    g_ctrl->stop = 0;
    g_ctrl->start = 0;

    memset(g_stats, 0, sizeof(struct rdmasrv_stat));

    for (i = 0; i < g_num_worker; i ++) {
        pid_t pid = fork();
        if (pid == 0) {
            cpu_set_t set;
            CPU_ZERO(&set);
            CPU_SET(i + 1, &set);
            sched_setaffinity(getpid(), sizeof(set), &set);
            g_slave_id = i;
            rdmasrv_run(g_port + i, g_buffer_size, g_num_qp, g_send, g_loop);
            return 0;
        } else if (pid > 0) {
            pids[i] = pid;
        }
    }
    g_ctrl->start = 1;

    while (g_ctrl->stop == 0) {
        sleep(g_stat_interval);
        int total_rx_tr = 0, total_tx_tr = 0, total_rx_b = 0, total_tx_b = 0;
        for (i = 0; i < g_num_worker; i ++) {
            total_rx_tr += g_stats[i].rx_transactions;
            total_tx_tr += g_stats[i].tx_transactions;
            total_rx_b += g_stats[i].rxbytes;
            total_tx_b += g_stats[i].txbytes;
        }
        fprintf(stdout, "TX Transactions: %u Rate: %lf\n", total_tx_tr, (double)((total_tx_tr - prev_tx_tr) / g_stat_interval));
        fprintf(stdout, "RX Transactions: %u Rate: %lf\n", total_rx_tr, (double)((total_rx_tr - prev_rx_tr) / g_stat_interval));
        fprintf(stdout, "RX Bytes: %u Rate: %lf Gbps\n", total_rx_b, (double)((double)((total_rx_b - prev_rx_b) / g_stat_interval) / 1000000000));
        fprintf(stdout, "TX Bytes: %u Rate: %lf Gbps\n", total_tx_b, (double)((double)((total_tx_b - prev_tx_b) / g_stat_interval) / 1000000000));
        prev_rx_tr = total_rx_tr;
        prev_tx_tr = total_tx_tr;
        prev_rx_b = total_rx_b;
        prev_tx_b = total_tx_b;
    }

    for (i = 0; i < g_num_worker; i++) {
        int status;
        waitpid(pids[i], &status, 0);
    }

    return 0;
}

