
#ifndef __RDMA_H__
#define __RDMA_H__

#include <rdma/rdma_cma.h>
#include "main.h"

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

	union {
    	struct ibv_mr mr;
  	} data;
};

struct rdma_connection {
    struct rdma_event_channel *ec;
    struct ibv_context *ctx;
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_comp_channel *comp_channel;
    struct rdma_cm_id *id;
    struct ibv_qp *qp;
    struct ibv_mr *recv_msg_mr;
    struct ibv_mr *send_msg_mr;
    struct ibv_mr *rdma_local_mr;
    struct ibv_mr peer_mr;
    struct message *recv_msg;
    struct message *send_msg;
    char *rdma_local_region;
    rdma_conn_state state;
    void *cc_event_opaque;
    void *ec_event_opaque;
    struct rdmacli_conf *conf;
    int loop;
};

#endif
