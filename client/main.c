
#include "main.h"

struct rdmacli_stat *g_stats;
struct rdmacli_slave_ctrl *g_ctrl;
struct rdmacli_conf *g_conf;

int g_slave_id;

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

static void parse_ipmap_substr(int slave, char *str)
{
    char delim[] = ",";
    char *ptr = strtok(str, delim);

    g_conf->ipmap[slave].num_ips = 0;

    while(ptr != NULL) {
        strcpy(g_conf->ipmap[slave].ip[g_conf->ipmap[slave].num_ips++], ptr);
        ptr = strtok(NULL, delim);
    }
}

static void parse_ipmap(char *str)
{
    char delim[] = ";";
    char *ptr = strtok(str, delim);
    char *tmp[RDMACLI_MAX_WORKER];
    int i;

    for (i = 0; i < RDMACLI_MAX_WORKER; i++)
        tmp[i] = NULL;

    while (ptr != NULL) {
        char *_tmp = malloc(strlen(ptr) + 1);
        int slave;
        sscanf(ptr, "%d@%s", &slave, _tmp);
        tmp[slave] = malloc(strlen(_tmp) + 1);
        strcpy(tmp[slave], _tmp);
        free(_tmp);
        ptr = strtok(NULL, delim);
    }

    for (i = 0; i < RDMACLI_MAX_WORKER; i++) {
        if (tmp[i] == NULL)
            continue;
        fprintf(stdout, "map str %s slave %d\n", tmp[i], i);
        parse_ipmap_substr(i, tmp[i]);
        free(tmp[i]);
    }
}

int main(int argc, char **argv)
{
    int i, opt, port, buffer_size, num_qp, num_worker, mode, stat_interval, duration, loop, num_ips = 0, ring_size;
    char host[64], client_start[64], *ipmap_str = NULL;
    pid_t pids[RDMASRV_MAX_WORKER];
	int total_duration = 0;
    unsigned long prev_rx_tr = 0, prev_tx_tr = 0, prev_rx_b = 0, prev_tx_b = 0;

    while((opt = getopt(argc, argv, "p:h:b:q:w:s:i:d:l:c:n:m:r:")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                break;
            case 'h':
                strcpy(host, optarg);
                break;
            case 'b':
                buffer_size = atoi(optarg);
                break;
            case 'q':
                num_qp = atoi(optarg);
                break;
            case 'w':
                num_worker = atoi(optarg);
                break;
            case 's':
                mode = atoi(optarg);
                break;
            case 'i':
                stat_interval = atoi(optarg);
                break;
            case 'd':
                duration = atoi(optarg);
                break;
            case 'l':
                loop = atoi(optarg);
                break;
            case 'c':
                strcpy(client_start, optarg);
                break;
            case 'n':
                num_ips = atoi(optarg);
                break;
            case 'm':
                ipmap_str = malloc(strlen(optarg) + 1);
                strcpy(ipmap_str, optarg);
                break;
            case 'r':
                ring_size = atoi(optarg);
                break;
            default:
                fprintf(stderr, "client -p <port> -h <destination IP> -b <buffer size> -q <num of queue pairs> -w <workers> -s <0: passive read 1: passive write 2: active read 3: active write> -i <stats interval> -d <duration> -l <loop 0: infinite> -c <start client IP> -n <number of client IPs> -m <ip map> -r <request ring size>\n");
                exit(100);
        }
    }

    signal(SIGCHLD, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGKILL, signal_handler);

    g_ctrl = mmap(NULL, sizeof(struct rdmacli_slave_ctrl), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (g_ctrl == NULL) {
        fprintf(stderr, "Failed to allocate slave ctrl message.\n");
        exit(-2);
    }

    g_stats = mmap(NULL, num_worker * sizeof(struct rdmacli_stat), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (g_stats == NULL) {
        fprintf(stderr, "Failed to allocate stats.\n");
        exit(-3);
    }

    g_conf = mmap(NULL, sizeof(struct rdmacli_conf), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (g_conf == NULL) {
        fprintf(stderr, "Failed to allocate conf.\n");
        exit(-4);
    } else {
        memset(g_conf, 0, sizeof(struct rdmacli_conf));
        g_conf->port = port;
        g_conf->buffer_size = buffer_size;
        g_conf->num_qp = num_qp;
        g_conf->num_worker = num_worker;
        g_conf->mode = mode;
        g_conf->stat_interval = stat_interval;
        g_conf->duration = duration;
        g_conf->loop = loop;
        strcpy(g_conf->host, host);
        g_conf->num_ips = num_ips;
        if (num_ips)
            strcpy(g_conf->client_start, client_start);
        if (ipmap_str) {
            parse_ipmap(ipmap_str);
            free(ipmap_str);
        }
        g_conf->ring_size = ring_size;
    }

    g_ctrl->stop = 0;
    g_ctrl->start = 0;

    memset(g_stats, 0, sizeof(struct rdmacli_stat));

    for (i = 0; i < num_worker; i ++) {
        pid_t pid = fork();
        if (pid == 0) {
            cpu_set_t set;
            CPU_ZERO(&set);
            CPU_SET(i, &set);
            sched_setaffinity(getpid(), sizeof(set), &set);
            g_slave_id = i;
            rdmacli_run();
            return 0;
        } else if (pid > 0) {
            pids[i] = pid;
        }
    }
    g_ctrl->start = 1;

    while (g_ctrl->stop == 0) {
        int i, total_rx_tr = 0, total_tx_tr = 0, total_rx_b = 0, total_tx_b = 0;
        sleep(stat_interval);
        for (i = 0; i < num_worker; i ++) {
            total_rx_tr += g_stats[i].rx_transactions;
            total_tx_tr += g_stats[i].tx_transactions;
            total_rx_b += g_stats[i].rxbytes;
            total_tx_b += g_stats[i].txbytes;
        }
        fprintf(stdout, "TX Transactions: %u Rate: %lf\n", total_tx_tr, (double)((total_tx_tr - prev_tx_tr) / stat_interval));
        fprintf(stdout, "RX Transactions: %u Rate: %lf\n", total_rx_tr, (double)((total_rx_tr - prev_rx_tr) / stat_interval));
        fprintf(stdout, "RX Bytes: %u Rate: %lf Gbps\n", total_rx_b, (double)((double)((total_rx_b - prev_rx_b) / stat_interval) / 1000000000));
        fprintf(stdout, "TX Bytes: %u Rate: %lf Gbps\n", total_tx_b, (double)((double)((total_tx_b - prev_tx_b) / stat_interval) / 1000000000));
        fprintf(stdout, "msg_mr_tx:\t");
        for (i = 0; i < num_worker; i++) {
            fprintf(stdout, "\t%lu", g_stats[i].msg_mr_tx);
        }
        fprintf(stdout, "\n");
        fprintf(stdout, "msg_mr_rx:\t");
        for (i = 0; i < num_worker; i++) {
            fprintf(stdout, "\t%lu", g_stats[i].msg_mr_rx);
        }
        fprintf(stdout, "\n");
        fprintf(stdout, "msg_done_tx:\t");
        for (i = 0; i < num_worker; i++) {
            fprintf(stdout, "\t%lu", g_stats[i].msg_done_tx);
        }
        fprintf(stdout, "\n");
        fprintf(stdout, "msg_done_rx:\t");
        for (i = 0; i < num_worker; i++) {
            fprintf(stdout, "\t%lu", g_stats[i].msg_done_rx);
        }
        fprintf(stdout, "\n");
        fprintf(stdout, "write attempt:\t");
        for (i = 0; i < num_worker; i++) {
            fprintf(stdout, "\t%lu", g_stats[i].write_attempt);
        }
        fprintf(stdout, "\n");
        fprintf(stdout, "write success:\t");
        for (i = 0; i < num_worker; i++) {
            fprintf(stdout, "\t%lu", g_stats[i].write_success);
        }
        fprintf(stdout, "\n");
        fprintf(stdout, "read attempt:\t");
        for (i = 0; i < num_worker; i++) {
            fprintf(stdout, "\t%lu", g_stats[i].read_attempt);
        }
        fprintf(stdout, "\n");
        fprintf(stdout, "read success:\t");
        for (i = 0; i < num_worker; i++) {
            fprintf(stdout, "\t%lu", g_stats[i].read_success);
        }
        fprintf(stdout, "\n");
        prev_rx_tr = total_rx_tr;
        prev_tx_tr = total_tx_tr;
        prev_rx_b = total_rx_b;
        prev_tx_b = total_tx_b;
        total_duration += stat_interval;
        if (total_duration >= duration) {
            g_ctrl->stop = 1;
            break;
        }
    }

    for (i = 0; i < num_worker; i++) {
        int status;
        waitpid(pids[i], &status, 0);
    }

    return 0;
}
