#include "xps_listener.h"

void listener_connection_handler(void *ptr);

// takes in epoll_fd, host as a string, port number and returns pointer to listening socket info.
// steps involved:
// step 1 : check host and port validity
// step 2 : create a socket and make it reuseable
// step 3 : setup listener address  using xps_getaddrinfo
// step 4 : bind the listening socket to that address
// step 5 : start listening on the port
// step 6 : create and allocate memory for a listener instance (xps_listener_t)
// step 7 : add socket to epoll using xps_loop_attach
// step 8 : add the socket to a global listeners list
// step 9 : return the listener instance
xps_listener_t *xps_listener_create(xps_core_t *core, const char *host, u_int port) {
  // checks to ensure both host and port are valid
  assert(core != NULL);
  assert(host != NULL);
  assert(is_valid_port(port)); 

  // Create socket instance
  int sock_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (sock_fd < 0) {
    logger(LOG_ERROR, "xps_listener_create()", "socket() failed");
    perror("Error message");
    return NULL;
  }

  // Make address reusable
  const int enable = 1;
  if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
    logger(LOG_ERROR, "xps_listener_create()", "setsockopt() failed");
    perror("Error message");
    close(sock_fd);
    return NULL;
  }

  // Setup listener address
  struct addrinfo *addr_info = xps_getaddrinfo(host, port); // Will be explained later
  if (addr_info == NULL) {
    logger(LOG_ERROR, "xps_listener_create()", "xps_getaddrinfo() failed");
    close(sock_fd);
    return NULL;
  }

  // Binding to port
  if (bind(sock_fd, addr_info->ai_addr, addr_info->ai_addrlen) < 0) {
    logger(LOG_ERROR, "xps_listener_create()", "failed to bind() to %s:%u", host, port);
    perror("Error message");
    freeaddrinfo(addr_info); // Will be explained later
    close(sock_fd);
    return NULL;
  }
  freeaddrinfo(addr_info); // Will be explained later

  // Listening on port
  if (listen(sock_fd, DEFAULT_BACKLOG) < 0) {
    logger(LOG_ERROR, "xps_listener_create()", "listen() failed");
    perror("Error message");
    close(sock_fd);
    return NULL;
  }

  // Create & allocate memory for a listener instance
  xps_listener_t *listener = malloc(sizeof(xps_listener_t));
  if (listener == NULL) {
    logger(LOG_ERROR, "xps_listener_create()", "malloc() failed for 'listener'");
    close(sock_fd);
    return NULL;
  }

  // Init values
  listener->core = core;
  listener->host = host;
  listener->port = port;
  listener->sock_fd = sock_fd;

  // Attach listener to loop
  xps_loop_attach(core->loop, sock_fd, EPOLLIN, listener, listener_connection_handler);

  // Add listener to global listeners list
  vec_push(&core->listeners, listener);

  logger(LOG_DEBUG, "xps_listener_create()", "created listener on port %d", port);

  return listener;
}

// takes in a listener instance
// step 1 : check listener validity
// step 2 : remove the socket from interest list
// step 3 : [IMPORTANT] set the entry to NULL in global listeners list instead of removing it
// step 4 : close the socket
// step 5 : free the listener instance
void xps_listener_destroy(xps_listener_t *listener) {

  // Validate params
  assert(listener != NULL);

  xps_core_t *core = listener->core;

  // Detach listener from loop
  xps_loop_detach(core->loop, listener->sock_fd);

  // Set listener to NULL in 'listeners' list
  for (int i = 0; i < core->listeners.length; i++) {
    xps_listener_t *curr = core->listeners.data[i];
    if (curr == listener) {
      core->listeners.data[i] = NULL;
      break;
    }
  }

  // Close socket
  close(listener->sock_fd);

  logger(LOG_DEBUG, "xps_listener_destroy()", "destroyed listener on port %d", listener->port);

  // Free listener instance
  free(listener);

}

// takes in listener instance
// step 1 : check listener instance validity
// step 2 : accept the incoming client request using accept()
// step 3 : create a connection instance using connection_create
void listener_connection_handler(void *ptr) {
  assert(ptr != NULL);
  xps_listener_t *listener = ptr;

  struct sockaddr conn_addr;
  socklen_t conn_addr_len = sizeof(conn_addr);

  // Accepting connection
  int conn_sock_fd = accept(listener->sock_fd, (struct sockaddr*)&conn_addr, &conn_addr_len);
  if (conn_sock_fd < 0) {
    logger(LOG_ERROR, "xps_listener_connection_handler()", "accept() failed");
    perror("Error message");
    return;
  }

  // Creating connection instance
  xps_connection_t *client = xps_connection_create(listener->core, conn_sock_fd); // Will be implemented later
  if (client == NULL) {
    logger(LOG_ERROR, "xps_listener_connection_handler()", "xps_connection_create() failed");
    close(conn_sock_fd);
    return;
  }
  client->listener = listener;

  logger(LOG_INFO, "xps_listener_connection_handler()", "new connection");
}