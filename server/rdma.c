
#include "event.h"
#include "rdma.h"

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
    sge.length = conn->prop->buffer_size;
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

static void rdmasrv_handle_cc_events(void *data, int event)
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
                    if ((conn->prop->mode == ACTIVE_WRITE || 
                            conn->prop->mode == ACTIVE_READ) && conn->state == CONN_CONNECTED) {
                        memcpy(&conn->peer_mr, &conn->recv_msg->data.mr, sizeof(conn->peer_mr));
                        conn->state = CONN_CONNECTED_MR_RECEIVED;
                        if (conn->prop->mode == ACTIVE_READ) {
                            //fprintf(stdout, "%s:%d Posting READ.\n", __func__, __LINE__);
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
                    if ((conn->prop->mode == PASSIVE_READ ||
                             conn->prop->mode == PASSIVE_WRITE) && conn->state == CONN_CONNECTED_MR_SENT) {
                        rdma_disconnect(conn->id);
                        conn->state = CONN_DISCONNECTING;
                    } else {
                        fprintf(stderr, "Recieved DONE in wrong state.\n");
                        continue;
                    }
                }
            } else if (wc.opcode & IBV_WC_RDMA_READ) {
                if (conn->prop->mode == ACTIVE_READ && 
                        (conn->state == CONN_CONNECTED_MR_RECEIVED || conn->state == CONN_CONNECTED_OPS_COMPLETED)) {
                    conn->state = CONN_CONNECTED_OPS_COMPLETED;
                    ++conn->loop;
                    if (conn->loop >= conn->prop->loop) {
                        //fprintf(stdout, "%s:%d Sending DONE.\n", __func__, __LINE__);
                        post_send_done(conn);
                    } else {
                        fprintf(stdout, "%s:%d Posting READ.\n", __func__, __LINE__);
                        post_read(conn);
                    }
                } else {
                    fprintf(stderr, "Recieved WC_RDMA_READ in wrong state.\n");
                    continue;
                }
            } else if (wc.opcode & IBV_WC_RDMA_WRITE) {
                if (conn->prop->mode == ACTIVE_WRITE && 
                        (conn->state == CONN_CONNECTED_MR_RECEIVED || conn->state == CONN_CONNECTED_OPS_COMPLETED)) {
                    conn->state = CONN_CONNECTED_OPS_COMPLETED;
                    ++conn->loop;
                    if (conn->prop->loop != 0 && conn->loop >= conn->prop->loop) {
                        //fprintf(stdout, "%s:%d Sending DONE.\n", __func__, __LINE__);
                        post_send_done(conn);
                    } else {
                        fprintf(stdout, "%s:%d Posting WRITE.\n", __func__, __LINE__);
                        post_write(conn);
                    }
                } else {
                    fprintf(stderr, "Recieved WC_RDMA_WRITE in wrong state.\n");
                    continue;
                }
            } else {
                if ((conn->prop->mode == PASSIVE_READ ||
                        conn->prop->mode == PASSIVE_WRITE) && conn->state == CONN_CONNECTED) {
                    fprintf(stdout, "%s:%d Posting recvs.\n", __func__, __LINE__);
                    post_receives(conn);
                    conn->state = CONN_CONNECTED_MR_SENT;
                } else if ((conn->prop->mode == ACTIVE_WRITE ||
                       conn->prop->mode == ACTIVE_READ) && conn->state == CONN_CONNECTED_OPS_COMPLETED) {
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

static int rdmasrv_on_connection_request(struct rdma_cm_id *id)
{
    int r = 0;
    struct rdma_connection *conn = NULL;
    struct ibv_qp_init_attr qp_attr;
    struct rdmasrv_listener_property *prop;
    unsigned rdma_local_mr_flag = 0; 

    /* build context */
    conn = malloc(sizeof(struct rdma_connection));
    memset(conn, 0, sizeof(struct rdma_connection));
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
    prop = id->context;
    conn->prop = prop;
    id->context = conn;

    /* init mr */
    conn->send_msg = malloc(sizeof(struct message));
    conn->recv_msg = malloc(sizeof(struct message));
    conn->rdma_local_region = malloc(prop->buffer_size);

    if (prop->mode == PASSIVE_READ) {
        rdma_local_mr_flag = IBV_ACCESS_REMOTE_READ;
    } else if (prop->mode == PASSIVE_WRITE) {
        rdma_local_mr_flag = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE;
    } else if (prop->mode == ACTIVE_READ) {
        rdma_local_mr_flag = IBV_ACCESS_LOCAL_WRITE;
    } else if (prop->mode == ACTIVE_WRITE) {
        rdma_local_mr_flag = 0;
    }
    TEST_Z(conn->send_msg_mr = ibv_reg_mr(conn->pd, conn->send_msg, sizeof(struct message), 0));
    TEST_Z(conn->recv_msg_mr = ibv_reg_mr(conn->pd, conn->recv_msg, sizeof(struct message), IBV_ACCESS_LOCAL_WRITE));
    TEST_Z(conn->rdma_local_mr = ibv_reg_mr(conn->pd, conn->rdma_local_region, prop->buffer_size, rdma_local_mr_flag));

    /* Add CQ fd to poll event */
    TEST_NZ(fcntl(conn->comp_channel->fd, F_SETFL, fcntl(conn->comp_channel->fd, F_GETFL) | O_NONBLOCK));
    conn->cc_event_opaque = rdmasrv_add_event(conn->comp_channel->fd, (void *)conn, rdmasrv_handle_cc_events);

    /* recv if active read / write */
    if (prop->mode == ACTIVE_READ || prop->mode == ACTIVE_WRITE) {
        fprintf(stdout, "%s:%d posting recv.\n", __func__, __LINE__);
        post_receives(conn);
    }

    /* Now accept */
    struct rdma_conn_param cm_params;
    memset(&cm_params, 0, sizeof(cm_params));
    cm_params.initiator_depth = cm_params.responder_resources = 1;
    cm_params.rnr_retry_count = 7;
    rdma_accept(id, &cm_params);

    return r;
}

static int rdmasrv_on_connect(struct rdma_cm_id *id)
{
    struct rdma_connection *conn = (struct rdma_connection *) id->context;
    if (conn->prop->mode == PASSIVE_WRITE || conn->prop->mode == PASSIVE_READ) {
        fprintf(stdout, "%s:%d Sending memory region.\n", __func__, __LINE__);
        post_send_mr(conn);
    }
    conn->state = CONN_CONNECTED;
    return 0;
}

static int rdmasrv_on_disconnect(struct rdma_cm_id *id)
{
    struct rdma_connection *conn = (struct rdma_connection *) id->context;
    rdmasrv_del_event(conn->cc_event_opaque);
    rdma_destroy_qp(conn->id);
    ibv_dereg_mr(conn->send_msg_mr);
    ibv_dereg_mr(conn->recv_msg_mr);
    ibv_dereg_mr(conn->rdma_local_mr);
    free(conn->send_msg);
    free(conn->recv_msg);
    free(conn->rdma_local_region);
    rdma_destroy_id(conn->id);
    free(conn);
    return 0;
}

static int rdmasrv_on_event(struct rdma_cm_event *event)
{
    int r = 0;

    if (event->event == RDMA_CM_EVENT_CONNECT_REQUEST) {
        fprintf(stdout, "RDMA_CM_EVENT_CONNECT_REQUEST received\n");
        r = rdmasrv_on_connection_request(event->id);
    }
    else if (event->event == RDMA_CM_EVENT_ESTABLISHED) {
        fprintf(stdout, "RDMA_CM_EVENT_ESTABLISHED received\n");
        r = rdmasrv_on_connect(event->id);
    }
    else if (event->event == RDMA_CM_EVENT_DISCONNECTED) {
        fprintf(stdout, "RDMA_CM_EVENT_DISCONNECTED received\n");
        r = rdmasrv_on_disconnect(event->id);
    }
    else {
        fprintf(stderr, "rdmasrv_on_event: unknown event");
        exit(-100);
    }

    return r;
}

static void rdmasrv_handle_ec_events(void *data, int ev)
{
    struct rdma_event_channel *ec = (struct rdma_event_channel *) data;
    struct    rdma_cm_event *event = NULL;

    if (rdma_get_cm_event(ec, &event) == 0) {
        struct rdma_cm_event event_copy;
        memcpy(&event_copy, event, sizeof(*event));
        rdma_ack_cm_event(event);
        if (rdmasrv_on_event(&event_copy)) {
            return;
        }
    }
}

void rdmasrv_run(int port, int buffer_size, int num_qp, rdmasrv_mode mode, int loop)
{
    struct sockaddr_in6 addr;
    struct rdma_cm_id *listener = NULL;
    struct rdma_event_channel *ec = NULL;
    struct rdmasrv_listener_property prop = {
        .buffer_size = buffer_size,
        .mode = mode,
        .loop = loop,
    };
    void *ec_event_opaque;

    rdmasrv_event_init();

    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);

    TEST_Z(ec = rdma_create_event_channel());
    TEST_NZ(rdma_create_id(ec, &listener, &prop, RDMA_PS_TCP));
    TEST_NZ(rdma_bind_addr(listener, (struct sockaddr *)&addr));
    TEST_NZ(rdma_listen(listener, 10));

    TEST_NZ(fcntl(ec->fd, F_SETFL, fcntl(ec->fd, F_GETFL) | O_NONBLOCK));
    ec_event_opaque = rdmasrv_add_event(ec->fd, (void *)ec, rdmasrv_handle_ec_events);

    while(g_ctrl->start == 0);
  
    while(g_ctrl->stop == 0)
        rdmasrv_handle_event();

    fprintf(stdout, "Exiting worker %d\n", g_slave_id);

    rdmasrv_del_event(ec_event_opaque);
    rdma_destroy_id(listener);
    rdma_destroy_event_channel(ec);
}
