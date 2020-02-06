#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "event.h"
#include "rdma.h"

#define MAX_CONNECTIONS 1024

struct rdma_event_channel *g_ec;
void *g_ec_event_opaque = NULL;
static struct rdma_connection *conns[MAX_CONNECTIONS];

static inline void post_receives(struct rdma_connection *conn)
{
    struct ibv_recv_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    wr.wr_id = (uintptr_t)conn;
    wr.next = NULL;
    wr.sg_list = &sge;
    wr.num_sge = 1;

    sge.addr = (uintptr_t)conn->recv_msg;
    sge.length = sizeof(struct message);
    sge.lkey = conn->recv_msg_mr->lkey;

    TEST_NZ(ibv_post_recv(conn->qp, &wr, &bad_wr));
}

static inline void post_send(struct rdma_connection *conn)
{
    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t)conn;
    wr.opcode = IBV_WR_SEND;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.send_flags = IBV_SEND_SIGNALED;

    sge.addr = (uintptr_t)conn->send_msg;
    sge.length = sizeof(struct message);
    sge.lkey = conn->send_msg_mr->lkey;

    TEST_NZ(ibv_post_send(conn->qp, &wr, &bad_wr));
}

static inline void post_send_mr(struct rdma_connection *conn)
{
    int i;
    conn->send_msg->type = MSG_MR;
    conn->send_msg->mr_num = conn->conf->ring_size;
    for (i = 0; i < conn->conf->ring_size; i ++)
        memcpy(&conn->send_msg->mr[i], conn->r_elem[i].rdma_local_mr, sizeof(struct ibv_mr));
    post_send(conn);
}

static inline void post_send_done(struct rdma_connection *conn)
{
    conn->send_msg->type = MSG_DONE;
    post_send(conn);
}

static inline int post_read_write(struct rdma_connection *conn, enum ibv_wr_opcode opcode, int idx)
{
    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));
    wr.wr_id = (uintptr_t)&conn->r_elem[idx];
    wr.opcode = opcode;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.wr.rdma.remote_addr = (uintptr_t)conn->r_elem[idx].peer_mr.addr;
    wr.wr.rdma.rkey = conn->r_elem[idx].peer_mr.rkey;

    sge.addr = (uintptr_t)conn->r_elem[idx].rdma_local_region;
    sge.length = conn->conf->buffer_size;
    sge.lkey = conn->r_elem[idx].rdma_local_mr->lkey;

    //fprintf(stdout, "[%d] posting read / write for idx %d\n", g_slave_id, idx);

    return ibv_post_send(conn->qp, &wr, &bad_wr);
}

static inline int post_read(struct rdma_connection *conn)
{
    int i, cnt = 0;

    for (i = 0; i < conn->conf->ring_size; i++) {
        if (conn->r_elem[i].free) {
            int ret = post_read_write(conn, IBV_WR_RDMA_READ, i);
            if (ret) {
                if (ret != -ENOMEM)
                    fprintf(stderr, "post_read_write() returned %d errno: %s\n", ret, strerror(errno));
                conn->r_elem[i].free = 1;
                //fprintf(stdout, "[1] Posted %d read requests.\n", cnt);
                return cnt;
            } else {
                cnt ++;
                conn->r_elem[i].free = 0;
                conn->r_elem_free --;
            }
        }
    }

    //fprintf(stdout, "[2] Posted %d read requests.\n", cnt);
    return cnt;
}

static inline int post_write(struct rdma_connection *conn)
{
    int i, cnt = 0;

    for (i = 0; i < conn->conf->ring_size; i++) {
        if (conn->r_elem[i].free) {
            int ret = post_read_write(conn, IBV_WR_RDMA_WRITE, i);
            if (ret) {
                if (ret != -ENOMEM)
                    fprintf(stderr, "post_read_write() returned %d errno: %s\n", ret, strerror(errno));
                conn->r_elem[i].free = 1;
                return cnt;
            } else {
                cnt ++;
                conn->r_elem[i].free = 0;
                conn->r_elem_free --;
            }
        }
    }

    return cnt;
}

static void rdmacli_handle_cc_events(void *data, int event)
{
    void *ctx;
    struct rdma_connection *conn = (struct rdma_connection *) data;
    struct ibv_cq *cq;
    struct ibv_wc wc;

    if (ibv_get_cq_event(conn->comp_channel, &cq, &ctx) == 0) {
        ibv_ack_cq_events(cq, 1);
        TEST_NZ(ibv_req_notify_cq(cq, 0));
        while (ibv_poll_cq(cq, 1, &wc)) {
            if (wc.status != IBV_WC_SUCCESS) {
                fprintf(stderr, "[%d] wc.status  != IBV_WC_SUCCESS %d\n", g_slave_id, wc.status);
                continue;
            }
            //fprintf(stdout, "Received cc opcode: %d\n", wc.opcode);
            if (wc.opcode & IBV_WC_RECV) {
                if (conn->recv_msg->type == MSG_MR) {
                    g_stats[g_slave_id].msg_mr_rx ++;
                    if ((conn->conf->mode == ACTIVE_WRITE || 
                            conn->conf->mode == ACTIVE_READ) && conn->state == CONN_CONNECTED) {
                        int i;
                        if (conn->recv_msg->mr_num > MAX_RING_SIZE) {
                            fprintf(stderr, "[%d] conn->recv_msg->mr_num(%u) > MAX_RING_SIZE\n", g_slave_id, conn->recv_msg->mr_num);
                            continue;
                        }

                        if (conn->recv_msg->mr_num != conn->conf->ring_size) {
                            fprintf(stderr, "[%d] Server and client configured with different ring size. server %u client %u. Dying.\n", g_slave_id, conn->recv_msg->mr_num, conn->conf->ring_size);
                            exit(0);
                        }
                        for (i = 0; i < conn->recv_msg->mr_num; i ++) {
                            memcpy(&conn->r_elem[i].peer_mr, &conn->recv_msg->mr[i], sizeof(struct ibv_mr));
                        }
                        conn->state = CONN_CONNECTED_MR_RECEIVED;
                        ++conn->loop;
                        if (conn->conf->mode == ACTIVE_READ) {
                            fprintf(stdout, "[%d] %s:%d Posting READ.\n", g_slave_id, __func__, __LINE__);
                            g_stats[g_slave_id].read_attempt += post_read(conn);
                        } else {
                            fprintf(stdout, "[%d] %s:%d Posting WRITE.\n", g_slave_id, __func__, __LINE__);
                            g_stats[g_slave_id].write_attempt += post_write(conn);
                        }
                    } else {
                        fprintf(stderr, "[%d] Recieved MR in wrong state.\n", g_slave_id);
                        continue;
                    }
                } else if (conn->recv_msg->type == MSG_DONE) {
                    g_stats[g_slave_id].msg_done_rx ++;
                    if ((conn->conf->mode == PASSIVE_READ ||
                             conn->conf->mode == PASSIVE_WRITE) && conn->state == CONN_CONNECTED_MR_SENT) {
                        rdma_disconnect(conn->id);
                        conn->state = CONN_DISCONNECTING;
                    } else {
                        fprintf(stderr, "[%d] Recieved DONE in wrong state.\n", g_slave_id);
                        continue;
                    }
                }
            } else if (wc.opcode & IBV_WC_RDMA_READ) {
                g_stats[g_slave_id].read_success ++;
                if (conn->conf->mode == ACTIVE_READ && 
                    (conn->state == CONN_CONNECTED_MR_RECEIVED || conn->state == CONN_CONNECTED_OPS_COMPLETED)) {
                    struct rdma_rqring_elem *elem = (struct rdma_rqring_elem *) wc.wr_id;
                    elem->free = 1;
                    conn->r_elem_free ++;
                    if (conn->r_elem_free > conn->conf->ring_size) {
                        die("conn->r_elem_free > conn->ring_size\n");
                    }
                    //fprintf(stdout, "Got ACK for element index %lu\n", (((uint64_t) elem) - ((uint64_t) conn->r_elem)) / sizeof(struct rdma_rqring_elem));
                    conn->state = CONN_CONNECTED_OPS_COMPLETED;
                    if (conn->conf->loop && conn->loop >= conn->conf->loop) {
                        if (conn->r_elem_free == conn->conf->ring_size) {
                            fprintf(stdout, "[%d] %s:%d Sending DONE.\n", g_slave_id, __func__, __LINE__);
                            post_send_done(conn);
                        }
                    } else {
                        //fprintf(stdout, "%s:%d Posting READ.\n", __func__, __LINE__);
                        ++conn->loop;
                        g_stats[g_slave_id].read_attempt += post_read(conn);
                    }
                } else {
                    fprintf(stderr, "[%d] Recieved WC_RDMA_READ in wrong state.\n", g_slave_id);
                    continue;
                }
            } else if (wc.opcode & IBV_WC_RDMA_WRITE) {
                g_stats[g_slave_id].write_success ++;
                if (conn->conf->mode == ACTIVE_WRITE && 
                    (conn->state == CONN_CONNECTED_MR_RECEIVED || conn->state == CONN_CONNECTED_OPS_COMPLETED)) {
                    struct rdma_rqring_elem *elem = (struct rdma_rqring_elem *) wc.wr_id;
                    elem->free = 1;
                    conn->r_elem_free ++;
                    if (conn->r_elem_free > conn->conf->ring_size) {
                        die("conn->r_elem_free > conn->ring_size\n");
                    }
                    //fprintf(stdout, "Got ACK for element index %lu\n", (((uint64_t) elem) - ((uint64_t) conn->r_elem)) / sizeof(struct rdma_rqring_elem));
                    conn->state = CONN_CONNECTED_OPS_COMPLETED;
                    if (conn->conf->loop && conn->loop >= conn->conf->loop) {
                        if (conn->r_elem_free == conn->conf->ring_size) {
                            fprintf(stdout, "[%d] %s:%d Sending DONE.\n", g_slave_id, __func__, __LINE__);
                            post_send_done(conn);
                        }
                    } else {
                        ++conn->loop;
                        //fprintf(stdout, "%s:%d Posting WRITE.\n", __func__, __LINE__);
                        g_stats[g_slave_id].write_attempt += post_write(conn);
                    }
                } else {
                    fprintf(stderr, "[%d] Recieved WC_RDMA_WRITE in wrong state. conn->conf->mode: %d conn->state: %d\n", g_slave_id, conn->conf->mode, conn->state);
                    continue;
                }
            } else {
                if ((conn->conf->mode == PASSIVE_READ ||
                        conn->conf->mode == PASSIVE_WRITE) && conn->state == CONN_CONNECTED) {
                    fprintf(stdout, "[%d] %s:%d Posting recvs.\n", g_slave_id, __func__, __LINE__);
                    post_receives(conn);
                    conn->state = CONN_CONNECTED_MR_SENT;
                    g_stats[g_slave_id].msg_mr_tx ++;
                } else if ((conn->conf->mode == ACTIVE_WRITE ||
                       conn->conf->mode == ACTIVE_READ) && conn->state == CONN_CONNECTED_OPS_COMPLETED) {
                    g_stats[g_slave_id].msg_done_tx ++;
                    rdma_disconnect(conn->id);
                    conn->state = CONN_DISCONNECTING;
                } else {
                    fprintf(stderr, "[%d] Recieved WC_SEND in wrong state.\n", g_slave_id);
                    continue;
                }
            }
        }
    }
}

static void rdmacli_on_disconnected(struct rdma_cm_id *id)
{
    struct rdma_connection *conn = id->context;
    int i;
    rdmacli_del_event(conn->cc_event_opaque);
    rdma_destroy_qp(conn->id);
    ibv_dereg_mr(conn->send_msg_mr);
    ibv_dereg_mr(conn->recv_msg_mr);
    for (i = 0; i < conn->conf->ring_size; i ++) {
        ibv_dereg_mr(conn->r_elem[i].rdma_local_mr);
        free(conn->r_elem[i].rdma_local_region);
    }
    free(conn->send_msg);
    free(conn->recv_msg);
    rdma_destroy_id(conn->id);
}

static void rdmacli_on_established(struct rdma_cm_id *id)
{
    struct rdma_connection *conn = id->context;
    conn->state = CONN_CONNECTED;
    if (conn->conf->mode == PASSIVE_WRITE || conn->conf->mode == PASSIVE_READ) {
        fprintf(stdout, "[%d] %s:%d Sending memory region.\n", g_slave_id, __func__, __LINE__);
        post_send_mr(conn);
    }
}

static void rdmacli_on_route_resolved(struct rdma_cm_id *id)
{
    struct rdma_conn_param cm_params;
    memset(&cm_params, 0, sizeof(cm_params));
    cm_params.initiator_depth = cm_params.responder_resources = 1;
    cm_params.rnr_retry_count = 7;
    cm_params.retry_count = 7;
    TEST_NZ(rdma_connect(id, &cm_params));
}

static void rdmacli_on_addr_resolved(struct rdma_cm_id *id)
{
    struct rdma_connection *conn = id->context;
    unsigned rdma_local_mr_flag = 0; 
    struct ibv_qp_init_attr qp_attr;
    int i;

    /* build context */
    conn->ctx = id->verbs;
    TEST_Z(conn->pd = ibv_alloc_pd(id->verbs));
    TEST_Z(conn->comp_channel = ibv_create_comp_channel(id->verbs));
    TEST_Z(conn->cq = ibv_create_cq(conn->ctx, 100, NULL, conn->comp_channel, 0));
    TEST_NZ(ibv_req_notify_cq(conn->cq, 0));

    /* init attr */
    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.send_cq = conn->cq;
    qp_attr.recv_cq = conn->cq;
    qp_attr.qp_type = IBV_QPT_RC;
    qp_attr.cap.max_send_wr = 10;
    qp_attr.cap.max_recv_wr = 10;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;

    /* create qp */
    TEST_NZ(rdma_create_qp(id, conn->pd, &qp_attr));

    conn->id = id;
    conn->qp = id->qp;
    conn->state = CONN_INIT;

    /* init mr */
    conn->send_msg = malloc(sizeof(struct message));
    conn->recv_msg = malloc(sizeof(struct message));
    for (i = 0; i < conn->conf->ring_size; i ++) {
        conn->r_elem[i].rdma_local_region = malloc(conn->conf->buffer_size);
        conn->r_elem[i].free = 1;
    }

    if (conn->conf->mode == PASSIVE_READ) {
        rdma_local_mr_flag = IBV_ACCESS_REMOTE_READ;
    } else if (conn->conf->mode == PASSIVE_WRITE) {
        rdma_local_mr_flag = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE;
    } else if (conn->conf->mode == ACTIVE_READ) {
        rdma_local_mr_flag = IBV_ACCESS_LOCAL_WRITE;
    } else if (conn->conf->mode == ACTIVE_WRITE) {
        rdma_local_mr_flag = 0;
    }
    TEST_Z(conn->send_msg_mr = ibv_reg_mr(conn->pd, conn->send_msg, sizeof(struct message), 0));
    TEST_Z(conn->recv_msg_mr = ibv_reg_mr(conn->pd, conn->recv_msg, sizeof(struct message), IBV_ACCESS_LOCAL_WRITE));
    for (i = 0; i < conn->conf->ring_size; i ++) {
        TEST_Z(conn->r_elem[i].rdma_local_mr = ibv_reg_mr(conn->pd, conn->r_elem[i].rdma_local_region, conn->conf->buffer_size, rdma_local_mr_flag | (1 << 7)));
    }
    conn->r_elem_free = conn->conf->ring_size;

    /* Add CQ fd to poll event */
    TEST_NZ(fcntl(conn->comp_channel->fd, F_SETFL, fcntl(conn->comp_channel->fd, F_GETFL) | O_NONBLOCK));
    conn->cc_event_opaque = rdmacli_add_event(conn->comp_channel->fd, (void *)conn, rdmacli_handle_cc_events);

    /* recv if active read / write */
    if (conn->conf->mode == ACTIVE_READ || conn->conf->mode == ACTIVE_WRITE) {
        fprintf(stdout, "[%d] %s:%d posting recv.\n", g_slave_id, __func__, __LINE__);
        post_receives(conn);
    }

    /* Resolve route */
    TEST_NZ(rdma_resolve_route(id, 500));
}

static void rdmacli_on_event(struct rdma_cm_event *event)
{
    if (event->event == RDMA_CM_EVENT_ADDR_RESOLVED) {
        fprintf(stdout, "[%d] RDMA_CM_EVENT_ADDR_RESOLVED received .. \n", g_slave_id);
        rdmacli_on_addr_resolved(event->id);
    }
    else if (event->event == RDMA_CM_EVENT_ROUTE_RESOLVED) {
        fprintf(stdout, "[%d] RDMA_CM_EVENT_ROUTE_RESOLVED received .. \n", g_slave_id);
        rdmacli_on_route_resolved(event->id);
    }
    else if (event->event == RDMA_CM_EVENT_ESTABLISHED) {
        fprintf(stdout, "[%d] RDMA_CM_EVENT_ESTABLISHED received .. \n", g_slave_id);
        rdmacli_on_established(event->id);
    }
    else if (event->event == RDMA_CM_EVENT_DISCONNECTED) {
        fprintf(stdout, "[%d] RDMA_CM_EVENT_DISCONNECTED .. \n", g_slave_id);
        rdmacli_on_disconnected(event->id);
    }
    else {
        fprintf(stderr, "[%d] on_event: %d\n", g_slave_id, event->event);
        die("on_event: unknown event.");
    }
}

static void rdmacli_handle_ec_events(void *data, int ev)
{
    struct rdma_cm_event *event;
    if (rdma_get_cm_event(g_ec, &event) == 0) {
        struct rdma_cm_event event_copy;
        memcpy(&event_copy, event, sizeof(*event));
        rdma_ack_cm_event(event);
        rdmacli_on_event(&event_copy);
    }
}

static char *increment_ip(char* address_string, int num)
{
    in_addr_t address = inet_addr(address_string);
    address = ntohl(address);
    address += num;
    address = htonl(address);
    struct in_addr address_struct;
    address_struct.s_addr = address;
    return inet_ntoa(address_struct);
}

void rdmacli_run(void)
{
    int i;
    struct addrinfo *addr, *cliaddr;
    char dst_port[64];

    rdmacli_event_init();

    while(g_ctrl->start == 0);
    fprintf(stdout, "[%d] Starting test ...\n", g_slave_id);
    if (g_conf->num_ips == 0 && g_conf->ipmap[g_slave_id].num_ips == 0) {
        fprintf(stdout, "[%d] no ipmap defined. in lazy loop\n", g_slave_id);
        while (g_ctrl->stop == 0)
            sleep(10);
        fprintf(stdout, "[%d] Exiting worker %d from lazy loop.\n", g_slave_id, g_slave_id);
        return;
    }

    snprintf(dst_port, 63, "%d", g_conf->port + g_slave_id);
    TEST_NZ(getaddrinfo(g_conf->host, dst_port, NULL, &addr));

    TEST_Z(g_ec = rdma_create_event_channel());
    TEST_NZ(fcntl(g_ec->fd, F_SETFL, fcntl(g_ec->fd, F_GETFL) | O_NONBLOCK));
    g_ec_event_opaque = rdmacli_add_event(g_ec->fd, NULL, rdmacli_handle_ec_events);

    for (i = 0; i < g_conf->num_qp; i++) {
        char cliip[64];
        conns[i] = malloc(sizeof(struct rdma_connection));
        memset(conns[i], 0, sizeof(struct rdma_connection));
        conns[i]->conf = g_conf;
        conns[i]->ec = g_ec;
        TEST_NZ(rdma_create_id(conns[i]->ec, &conns[i]->id, conns[i], RDMA_PS_TCP));
        if (g_conf->num_ips == 0 && g_conf->ipmap[g_slave_id].num_ips > 0) {
            int ipindex = i % g_conf->ipmap[g_slave_id].num_ips;
            strcpy(cliip, g_conf->ipmap[g_slave_id].ip[ipindex]);
        } else {
            sprintf(cliip, "%s", increment_ip(g_conf->client_start, (g_slave_id + (g_conf->num_worker * i)) % g_conf->num_ips));
        }
        fprintf(stdout, "[%d] conn id: %d source IP: %s\n", g_slave_id, i, cliip);
        TEST_NZ(getaddrinfo(cliip, NULL, NULL, &cliaddr));
        TEST_NZ(rdma_resolve_addr(conns[i]->id, cliaddr->ai_addr, addr->ai_addr, 500));
    }

    while(g_ctrl->stop == 0)
        rdmacli_handle_event();

    fprintf(stdout, "[%d] Exiting worker %d\n", g_slave_id, g_slave_id);

    rdmacli_del_event(g_ec_event_opaque);

    for (i = 0; i < g_conf->num_qp; i++) {
        free(conns[i]);
    }

    freeaddrinfo(addr);
}
