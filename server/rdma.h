
#ifndef __RDMA_H__
#define __RDMA_H__

#include <rdma/rdma_cma.h>
#include "main.h"

#define MAX_RING_SIZE 64

typedef enum {
    CONN_INIT = 0,
    CONN_CONNECTED,
    CONN_CONNECTED_MR_RECEIVED,
    CONN_CONNECTED_MR_SENT,
    CONN_CONNECTED_OPS_COMPLETED,
    CONN_DISCONNECTING,
    CONN_DISCONNECTED
} rdma_conn_state;

struct message {
	enum {
		MSG_MR,
		MSG_DONE
	} type;
    uint32_t mr_num;
    struct ibv_mr mr[MAX_RING_SIZE];
};

struct rdma_rqring_elem {
    struct ibv_mr *rdma_local_mr;
    char *rdma_local_region;
    struct ibv_mr peer_mr;
    int free;
};

struct rdma_connection {
    struct ibv_context *ctx;
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_comp_channel *comp_channel;
    struct rdma_cm_id *id;
    struct ibv_qp *qp;
    struct ibv_mr *recv_msg_mr;
    struct ibv_mr *send_msg_mr;
    struct message *recv_msg;
    struct message *send_msg;
	struct rdma_rqring_elem r_elem[MAX_RING_SIZE];
    rdma_conn_state state;
    void *cc_event_opaque;
    rdmasrv_listener_property_t *prop;
    int loop;
};

#endif
