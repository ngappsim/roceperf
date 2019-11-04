#ifndef __EVENT_H__
#define __EVENT_H__

#include <sys/epoll.h>
#include "main.h"

typedef void (*rdmasrv_event_handler_t)(void *data, int event);

extern void rdmasrv_event_init(void);
extern void *rdmasrv_add_event(int fd, void *data, rdmasrv_event_handler_t func);
extern void rdmasrv_del_event(void *opaque);
extern void rdmasrv_handle_event(void);

#endif
