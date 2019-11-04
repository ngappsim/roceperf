
#include "event.h"

struct event_struct {
    int fd;
    void *data;
    rdmasrv_event_handler_t func;
};

int g_epoll_fd = -1;

void rdmasrv_event_init(void)
{
    g_epoll_fd = epoll_create1(0);
}

void *rdmasrv_add_event(int fd, void *data, rdmasrv_event_handler_t func)
{
    struct epoll_event ev;
    struct event_struct *event;

    event = malloc(sizeof(struct event_struct));
    event->data = data;
    event->func = func;
    event->fd = fd;

    ev.events = EPOLLIN;
    ev.data.fd = fd;
    ev.data.ptr = event;

    epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, fd, &ev);

    return event;
}

void rdmasrv_del_event(void *opaque)
{
    struct event_struct *event = opaque;
    struct epoll_event ev;

    ev.data.fd = event->fd;
    epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, event->fd, &ev);
    free(event);
}

void rdmasrv_handle_event(void)
{
    struct epoll_event events[1024];
    int nfds;

    nfds = epoll_wait(g_epoll_fd, events, 1024, 1);
    for (int i = 0; i < nfds; i++) {
        struct event_struct *event = events[i].data.ptr;
        event->func(event->data, EPOLLIN);
    }
}

