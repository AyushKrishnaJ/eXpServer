#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/epoll.h>

#define PORT 8080
#define BUFF_SIZE 10000
#define MAX_ACCEPT_BACKLOG 5
#define MAX_EPOLL_EVENTS 10

void strrev(char* str) {
    for (int start = 0, end = strlen(str) - 2; start < end; start++, end--) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
    }
}

int main () {
    // creating a listening socket
    int listen_sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    // reusablity of the same port
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

    // create a event epoll
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    struct epoll_event event, events[MAX_EPOLL_EVENTS];

    // add listening sock to interest list
    event.events = EPOLLIN;
    event.data.fd = listen_sock_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock_fd, &event);

    while(1) {
        // epoll wait for entry in ready list (indefinitely since -1 is passed for timeout parameter)
        printf("[DEBUG] Epoll wait\n");
        int n_ready_fds = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, -1);

        // 2 cases for the fds either connection or listening)
        for (int i = 0; i < n_ready_fds; i++) {
            int curr_fd = events[i].data.fd;

            // if listening socket
            if (curr_fd == listen_sock_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof(client_addr);

                // step 1 : accept and create a connection socket
                int conn_sock_fd = accept(listen_sock_fd, (struct sockaddr*)&client_addr, &client_addr_len);
                printf("[INFO] Client has been connected\n");

                // step 2 : add the fd to epoll interest list
                event.events = EPOLLIN;
                event.data.fd = conn_sock_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_sock_fd, &event);
            }

            // if connection socket
            else {
                // step 1 : make a buffer
                char buff[BUFF_SIZE];
                memset(buff, 0, BUFF_SIZE);
                
                // step 2 : read the data into the buffer we just created
                ssize_t read_n = recv(curr_fd, buff, sizeof(buff), 0);

                // case 1 : if recv faced an error, close the server
                if (read_n < 0) {
                    printf("[INFO] Error occured. Closing server\n");
                    exit(EXIT_FAILURE);                    
                }

                // case 2 : if recv recieved size 0 implies client closed the connection, then remove fd from interest list and close the connection fd
                else if (read_n == 0) {
                    printf("[INFO] Client Disconnected\n");
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, curr_fd, NULL);
                    close(curr_fd);
                }

                // case 3 : print the client message, reverse it, and send it back to the client
                else {
                    printf("[CLIENT MESSAGE] %s", buff);
                    strrev(buff);

                    send(curr_fd, buff, read_n, 0);
                }
            }
        }
    }
}