#ifndef XPS_LISTENER_H
#define XPS_LISTENER_H

#include "../xps.h"

struct xps_listener_s {
  // int epoll_fd; // epoll FD that the listener is attached to
  xps_core_t *core;
  const char *host; // string IP address of host the listener is bound to
  u_int port; // port the server is listening on
  u_int sock_fd; // FD of the listening socket
};

// xps_listener_t *xps_listener_create(int epoll_fd, const char *host, u_int port);
xps_listener_t *xps_listener_create(xps_core_t *core, const char *host, u_int port);
void xps_listener_destroy(xps_listener_t *listener);
// void xps_listener_connection_handler(xps_listener_t *listener);

#endif