#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "event.h"
#include "rdma.h"

#define MAX_CONNECTIONS 1024

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
    conn->send_msg->type = MSG_MR;
    memcpy(&conn->send_msg->data.mr, conn->rdma_local_mr, sizeof(struct ibv_mr));
    post_send(conn);
}

static inline void post_send_done(struct rdma_connection *conn)
{
    conn->send_msg->type = MSG_DONE;
    post_send(conn);
}

static inline void post_read_write(struct rdma_connection *conn, enum ibv_wr_opcode opcode)
{
    struct ibv_send_wr wr, *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));
    wr.wr_id = (uintptr_t)conn;
    wr.opcode = opcode;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.wr.rdma.remote_addr = (uintptr_t)conn->peer_mr.addr;
    wr.wr.rdma.rkey = conn->peer_mr.rkey;

    sge.addr = (uintptr_t)conn->rdma_local_region;
    sge.length = conn->conf->buffer_size;
    sge.lkey = conn->rdma_local_mr->lkey;

    TEST_NZ(ibv_post_send(conn->qp, &wr, &bad_wr));
}

static inline void post_read(struct rdma_connection *conn)
{
    post_read_write(conn, IBV_WR_RDMA_READ);
}

static inline void post_write(struct rdma_connection *conn)
{
    post_read_write(conn, IBV_WR_RDMA_WRITE);
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
                fprintf(stderr, "wc.status  != IBV_WC_SUCCESS %d\n", wc.status);
                continue;
            }
            //fprintf(stdout, "Received cc opcode: %d\n", wc.opcode);
            if (wc.opcode & IBV_WC_RECV) {
                if (conn->recv_msg->type == MSG_MR) {
                    if ((conn->conf->mode == ACTIVE_WRITE || 
                            conn->conf->mode == ACTIVE_READ) && conn->state == CONN_CONNECTED) {
                        memcpy(&conn->peer_mr, &conn->recv_msg->data.mr, sizeof(conn->peer_mr));
                        conn->state = CONN_CONNECTED_MR_RECEIVED;
                        if (conn->conf->mode == ACTIVE_READ) {
                        fprintf(stdout, "%s:%d Posting READ.\n", __func__, __LINE__);
                            post_read(conn);
                        } else {
                        fprintf(stdout, "%s:%d Posting WRITE.\n", __func__, __LINE__);
                            post_write(conn);
                        }
                    } else {
                        fprintf(stderr, "Recieved MR in wrong state.\n");
                        continue;
                    }
                } else if (conn->recv_msg->type == MSG_DONE) {
                    if ((conn->conf->mode == PASSIVE_READ ||
                             conn->conf->mode == PASSIVE_WRITE) && conn->state == CONN_CONNECTED_MR_SENT) {
                        rdma_disconnect(conn->id);
                        conn->state = CONN_DISCONNECTING;
                    } else {
                        fprintf(stderr, "Recieved DONE in wrong state.\n");
                        continue;
                    }
                }
            } else if (wc.opcode & IBV_WC_RDMA_READ) {
                if (conn->conf->mode == ACTIVE_READ && 
                    (conn->state == CONN_CONNECTED_MR_RECEIVED || conn->state == CONN_CONNECTED_OPS_COMPLETED)) {
                    conn->state = CONN_CONNECTED_OPS_COMPLETED;
                    ++conn->loop;
                    if (conn->conf->loop && conn->loop >= conn->conf->loop) {
                        fprintf(stdout, "%s:%d Sending DONE.\n", __func__, __LINE__);
                        post_send_done(conn);
                    } else {
                        //fprintf(stdout, "%s:%d Posting READ.\n", __func__, __LINE__);
                        post_read(conn);
                    }
                } else {
                    fprintf(stderr, "Recieved WC_RDMA_READ in wrong state.\n");
                    continue;
                }
            } else if (wc.opcode & IBV_WC_RDMA_WRITE) {
                if (conn->conf->mode == ACTIVE_WRITE && 
                    (conn->state == CONN_CONNECTED || conn->state == CONN_CONNECTED_OPS_COMPLETED)) {
                    conn->state = CONN_CONNECTED_OPS_COMPLETED;
                    ++conn->loop;
                    if (conn->loop >= conn->conf->loop) {
                        fprintf(stdout, "%s:%d Sending DONE.\n", __func__, __LINE__);
                        post_send_done(conn);
                    } else {
                        //fprintf(stdout, "%s:%d Posting WRITE.\n", __func__, __LINE__);
                        post_write(conn);
                    }
                } else {
                    fprintf(stderr, "Recieved WC_RDMA_WRITE in wrong state.\n");
                    continue;
                }
            } else {
                if ((conn->conf->mode == PASSIVE_READ ||
                        conn->conf->mode == PASSIVE_WRITE) && conn->state == CONN_CONNECTED) {
                    fprintf(stdout, "%s:%d Posting recvs.\n", __func__, __LINE__);
                    post_receives(conn);
                    conn->state = CONN_CONNECTED_MR_SENT;
                } else if ((conn->conf->mode == ACTIVE_WRITE ||
                       conn->conf->mode == ACTIVE_READ) && conn->state == CONN_CONNECTED_OPS_COMPLETED) {
                    rdma_disconnect(conn->id);
                    conn->state = CONN_DISCONNECTING;
                } else {
                    fprintf(stderr, "Recieved WC_SEND in wrong state.\n");
                    continue;
                }
            }
        }
    }
}

static void rdmacli_on_disconnected(struct rdma_cm_id *id)
{
    struct rdma_connection *conn = id->context;
    rdmacli_del_event(conn->cc_event_opaque);
    rdma_destroy_qp(conn->id);
    ibv_dereg_mr(conn->send_msg_mr);
    ibv_dereg_mr(conn->recv_msg_mr);
    ibv_dereg_mr(conn->rdma_local_mr);
    free(conn->send_msg);
    free(conn->recv_msg);
    free(conn->rdma_local_region);
    rdma_destroy_id(conn->id);
}

static void rdmacli_on_established(struct rdma_cm_id *id)
{
    struct rdma_connection *conn = id->context;
    conn->state = CONN_CONNECTED;
    if (conn->conf->mode == PASSIVE_WRITE || conn->conf->mode == PASSIVE_READ) {
        fprintf(stdout, "%s:%d Sending memory region.\n", __func__, __LINE__);
        post_send_mr(conn);
    }
}

static void rdmacli_on_route_resolved(struct rdma_cm_id *id)
{
    struct rdma_conn_param cm_params;
    memset(&cm_params, 0, sizeof(cm_params));
    cm_params.initiator_depth = cm_params.responder_resources = 1;
    cm_params.rnr_retry_count = 7;
    TEST_NZ(rdma_connect(id, &cm_params));
}

static void rdmacli_on_addr_resolved(struct rdma_cm_id *id)
{
    struct rdma_connection *conn = id->context;
    unsigned rdma_local_mr_flag = 0; 
    struct ibv_qp_init_attr qp_attr;

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
    conn->rdma_local_region = malloc(conn->conf->buffer_size);

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
    TEST_Z(conn->rdma_local_mr = ibv_reg_mr(conn->pd, conn->rdma_local_region, conn->conf->buffer_size, rdma_local_mr_flag));

    /* Add CQ fd to poll event */
    TEST_NZ(fcntl(conn->comp_channel->fd, F_SETFL, fcntl(conn->comp_channel->fd, F_GETFL) | O_NONBLOCK));
    conn->cc_event_opaque = rdmacli_add_event(conn->comp_channel->fd, (void *)conn, rdmacli_handle_cc_events);

    /* recv if active read / write */
    if (conn->conf->mode == ACTIVE_READ || conn->conf->mode == ACTIVE_WRITE) {
        fprintf(stdout, "%s:%d posting recv.\n", __func__, __LINE__);
        post_receives(conn);
    }

    /* Resolve route */
    TEST_NZ(rdma_resolve_route(id, 500));
}

static void rdmacli_on_event(struct rdma_cm_event *event)
{
    if (event->event == RDMA_CM_EVENT_ADDR_RESOLVED) {
        fprintf(stdout, "RDMA_CM_EVENT_ADDR_RESOLVED received .. \n");
        rdmacli_on_addr_resolved(event->id);
    }
    else if (event->event == RDMA_CM_EVENT_ROUTE_RESOLVED) {
        fprintf(stdout, "RDMA_CM_EVENT_ROUTE_RESOLVED received .. \n");
        rdmacli_on_route_resolved(event->id);
    }
    else if (event->event == RDMA_CM_EVENT_ESTABLISHED) {
        fprintf(stdout, "RDMA_CM_EVENT_ESTABLISHED received .. \n");
        rdmacli_on_established(event->id);
    }
    else if (event->event == RDMA_CM_EVENT_DISCONNECTED) {
        fprintf(stdout, "RDMA_CM_EVENT_DISCONNECTED .. \n");
        rdmacli_on_disconnected(event->id);
    }
    else {
        fprintf(stderr, "on_event: %d\n", event->event);
        die("on_event: unknown event.");
    }
}

static void rdmacli_handle_ec_events(void *data, int ev)
{
    struct rdma_connection *conn = data;
    struct rdma_cm_event *event;
    if (rdma_get_cm_event(conn->ec, &event) == 0) {
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
    fprintf(stdout, "Starting test ...\n");

    snprintf(dst_port, 63, "%d", g_conf->port + g_slave_id);
    TEST_NZ(getaddrinfo(g_conf->host, dst_port, NULL, &addr));

    for (i = 0; i < g_conf->num_qp; i++) {
        char cliip[64];
        conns[i] = malloc(sizeof(struct rdma_connection));
        memset(conns[i], 0, sizeof(struct rdma_connection));
        conns[i]->conf = g_conf;
        TEST_Z(conns[i]->ec = rdma_create_event_channel());
        TEST_NZ(rdma_create_id(conns[i]->ec, &conns[i]->id, conns[i], RDMA_PS_TCP));
        TEST_NZ(fcntl(conns[i]->ec->fd, F_SETFL, fcntl(conns[i]->ec->fd, F_GETFL) | O_NONBLOCK));
        conns[i]->ec_event_opaque = rdmacli_add_event(conns[i]->ec->fd, (void *)conns[i], rdmacli_handle_ec_events);
        sprintf(cliip, "%s", increment_ip(g_conf->client_start, i % g_conf->num_ips));
        fprintf(stdout, "conn id: %d source IP: %s\n", i, cliip);
        TEST_NZ(getaddrinfo(cliip, NULL, NULL, &cliaddr));
        TEST_NZ(rdma_resolve_addr(conns[i]->id, cliaddr->ai_addr, addr->ai_addr, 500));
    }

    while(g_ctrl->stop == 0)
        rdmacli_handle_event();

    fprintf(stdout, "Exiting worker %d\n", g_slave_id);

    for (i = 0; i < g_conf->num_qp; i++) {
        free(conns[i]);
    }

    freeaddrinfo(addr);
}
