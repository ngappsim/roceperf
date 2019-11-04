#ifndef __EVENT_H__
#define __EVENT_H__

#include <sys/epoll.h>
#include "main.h"

typedef void (*rdmacli_event_handler_t)(void *data, int event);

extern void rdmacli_event_init(void);
extern void *rdmacli_add_event(int fd, void *data, rdmacli_event_handler_t func);
extern void rdmacli_del_event(void *opaque);
extern void rdmacli_handle_event(void);

#endif
