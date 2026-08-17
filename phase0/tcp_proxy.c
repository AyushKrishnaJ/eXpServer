#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>

#define PORT 8080
#define BUFF_SIZE 10000
#define MAX_ACCEPT_BACKLOG 5
#define MAX_EPOLL_EVENTS 10
#define UPSTREAM_PORT 3000
#define MAX_SOCKS 10
#define CONNECTION_SOCKET_TYPE 1
#define UPSTREAM_SOCKET_TYPE 2

int listen_sock_fd, epoll_fd;
struct epoll_event events[MAX_EPOLL_EVENTS];
int route_table[MAX_SOCKS][2], route_table_size = 0;

void strrev(char *str) {
    for (int start = 0, end = strlen(str) - 2; start < end; start++, end--) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
    }
}

// attach fd to epoll instance
void loop_attach(int epoll_fd, int fd, int events) {
    struct epoll_event event;
    event.events = events;
    event.data.fd = fd;

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
}

// check whether the fd passed is a connection socket (using the routing table) 

// find the corresponding upstream socket of the connection socket from route_table and vice versa
int find_socket(int fd, int type) {
    if (type == CONNECTION_SOCKET_TYPE) {
        for (int i = 0; i < route_table_size; i++) {
            if (route_table[i][0] == fd) return route_table[i][1];
        }
    }

    else if (type == UPSTREAM_SOCKET_TYPE) {
        for (int i = 0; i < route_table_size; i++) {
            if (route_table[i][1] == fd) return route_table[i][0];
        }        
    }
    return -1;
}

// connect to upstream server
int connect_upstream() {
    int upstream_sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in upstream_addr;
    upstream_addr.sin_family = AF_INET;
    upstream_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    upstream_addr.sin_port = htons(UPSTREAM_PORT);

    connect(upstream_sock_fd, (struct sockaddr*)&upstream_addr, sizeof(upstream_addr));

    return upstream_sock_fd;
}

// accept client connection
void accept_connection(int listen_sock_fd) {
    struct sockaddr_in client_addr;
    socklen_t sock_addr_len = sizeof(client_addr);
    int conn_sock_fd = accept(listen_sock_fd, (struct sockaddr*)&client_addr, &sock_addr_len);

    loop_attach(epoll_fd, conn_sock_fd, EPOLLIN);

    int upstream_sock_fd = connect_upstream();

    loop_attach(epoll_fd, upstream_sock_fd, EPOLLIN);

    route_table[route_table_size][0] = conn_sock_fd;
    route_table[route_table_size][1] = upstream_sock_fd;
    route_table_size++;
}

// handle client
void handle_client(int conn_sock_fd) {
    char buff[BUFF_SIZE];
    memset(buff, 0, BUFF_SIZE);

    ssize_t read_n = recv(conn_sock_fd, buff, BUFF_SIZE, 0);

    if (read_n < 0) {
        printf("[INFO] Error occured. Closing server\n");
        exit(EXIT_FAILURE);
    }

    else if (read_n == 0) {
        printf("[INFO] Client Disconnected\n");
        close(conn_sock_fd);
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn_sock_fd, NULL);
        return;
    }

    else {
        printf("[CLIENT MESSAGE] %s\n", buff);

        int upstream_sock_fd = find_socket(conn_sock_fd, CONNECTION_SOCKET_TYPE);
        
        // thing to note is send returns the number of bytes copied from user buffer
        // considerable difference in how we send compared to previous stages
        // in previous stage we just send whatever was in the buffer to the client
        // here
        int byte_written = 0;
        int message_len = read_n;
        while(byte_written < message_len) {
            int n = send(upstream_sock_fd, buff + byte_written, message_len - byte_written, 0);
            byte_written += n;
            printf("[DEBUG] n = %d\n", n);
        }
    }
}

// handle upstream
void handle_upstream(int upstream_sock_fd) {
    char buff[BUFF_SIZE];
    memset(buff, 0, BUFF_SIZE);

    ssize_t read_n = recv(upstream_sock_fd, buff, BUFF_SIZE, 0);

    if (read_n < 0) {
        printf("[INFO] Error occured. Failed to receive data from server\n");
        exit(EXIT_FAILURE);
    }

    else if (read_n == 0) {
        printf("[INFO] Connection closed by Server\n");
        close(upstream_sock_fd);
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, upstream_sock_fd, NULL);
        return;
    }

    else {
        int conn_sock_fd = find_socket(upstream_sock_fd, UPSTREAM_SOCKET_TYPE);
        
        // thing to note is send returns the number of bytes copied from user buffer
        // considerable difference in how we send compared to previous stages
        // in previous stage we just send whatever was in the buffer to the client
        // here
        int byte_written = 0;
        int message_len = read_n;
        while(byte_written < message_len) {
            int n = send(conn_sock_fd, buff + byte_written, message_len - byte_written, 0);
            byte_written += n;
            printf("[DEBUG] n = %d\n", n);
        }
    }

}

// create an epoll instance
int create_loop() {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    return epoll_fd;
}

// create listening socket and return it
int create_server() {
    // create a socket
    int listen_sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    // reusability
    int enable = 1;
    setsockopt(listen_sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

    // binding step
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);
    bind(listen_sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // listen on port 8080
    listen(listen_sock_fd, MAX_ACCEPT_BACKLOG);
    printf("[INFO] Server listening on port %d\n", PORT);

    return listen_sock_fd;
}

// infinite loop and handlling all epoll events
void loop_run(int epoll_fd) {
    while(1) {
        printf("[DEBUG] Epoll wait\n");
        int n_ready_fds = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, -1);
    
        for (int i = 0; i < n_ready_fds; i++) {
            int curr_fd = events[i].data.fd;
            // 3 possible cases

            // case 1 : event is on listen socket
            if (curr_fd == listen_sock_fd) {
                accept_connection(curr_fd);
            }

            // case 2 : event is on connection socket
            else if(find_socket(curr_fd, CONNECTION_SOCKET_TYPE) != -1) {
                handle_client(curr_fd);
            }

            // case 3 : event is on upstream socket
            else {
                handle_upstream(curr_fd);
            }
        }
    }
}

// create_server() :- creates a listening socket and returns its fd
// create_loop() :- creates a epoll instance and returns its fd
// loop_attach() :- add a fd into the epoll interest list
// loop_run() :- epoll wait loop, 3 cases - listen sock, conn sock, upstream sock
// connect_upstream() :- used to create an upstream connection and finally add the entry in the routing table
// find_socket() :- find corresponding upstream socket for connection socket and vice versa

// case 1 : Listening Socket
// accept_connection() :- accept client connection, loop_attach() it to epoll instance, connect to upstream, loop_attach() upstream fd to epoll instance, update routing table 

// case 2 : Connection Socket
// handle_client() :- recv what the client is trying to send through the connection socket, find the corresponding upstream socket through the routing table, send the message of the client to the upstream server

// case 3 : Upstream Socket
// handle_upstream() :- similar as handle_client but from the other perspective

int main() {
    listen_sock_fd = create_server();

    epoll_fd = create_loop();

    loop_attach(epoll_fd, listen_sock_fd, EPOLLIN);

    loop_run(epoll_fd);
}