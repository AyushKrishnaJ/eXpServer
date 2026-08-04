#include <arpa/inet.h> // htons, htonl, inet_ntoa, etc. -G byte-order & address conversion
#include <netdb.h>
#include <netinet/in.h> // struct sockaddr_in
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h> // socket, bind, listen, accept, send, recv, setsockopt
#include <unistd.h>

#define PORT 8080
#define BUFF_SIZE 10000
#define MAX_ACCEPT_BACKLOG 5

// -2 to avoid the '\n' character at the end of the string 
void strrev(char *str) {
    for (int start = 0, end = strlen(str) - 2; start < end; start++, end--) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
    }
}

int main() {
    // creating listening socket
    // socket : Returns -1 for error, fd for success
    // parameter :- domain (AF_INET, AF_INET6), type(SOCK_STREAM, SOCK_DGRAM), protocol(0 takes the default protocol assosciated with the domain and type)
    int listen_sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    // setting sock opt reuse addr
    // this is used for cases where closes the server puts the socket in TIME_WAIT state and instantly restarting the server results in an error. Adding this allows the reuse of those type of sockets
    int enable = 1;
    setsockopt(listen_sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

    // Builos address to bind to
    // htonl stands for host to network long
    // htons stands for host to network short
    // INADDR_ANY (0.0.0.0)
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);
    // printf("family=%d port=%u addr=%s len=%u\n",
    //    server_addr.sin_family,
    //    ntohs(server_addr.sin_port),
    //    inet_ntoa(server_addr.sin_addr),
    //    sizeof(server_addr));

    // bind the socket to the ip, port
    bind(listen_sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // start to listen
    listen(listen_sock_fd, MAX_ACCEPT_BACKLOG);
    printf("[INFO] Server listening on port %d\n", PORT);


    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    int conn_sock_fd = accept(listen_sock_fd, (struct sockaddr*)&client_addr, &client_addr_len);
    printf("[INFO] Client connected to server\n");
    // printf("family=%d port=%u addr=%s len=%u\n",
    //    client_addr.sin_family,
    //    ntohs(client_addr.sin_port),
    //    inet_ntoa(client_addr.sin_addr),
    //    client_addr_len);

    while (1)   
    {
        char buff[BUFF_SIZE];
        memset(buff, 0, BUFF_SIZE); // initialize value of buff to 0

        // read message from client to buffer
        ssize_t read_n = recv(conn_sock_fd, buff, sizeof(buff), 0);

        // client closed connection or error occurred
        if (read_n < 0) {
            printf("[INFO] Error occured. Closing server\n");
            close(conn_sock_fd);
            exit(1);
        }

        else if (read_n == 0) {
            printf("[INFO] Client Disconnected. Closing server\n");
            close(conn_sock_fd);
            exit(1);
        }

        printf("[CLIENT MESSAGE] %s", buff);
        strrev(buff);

        send(conn_sock_fd, buff, read_n, 0);
    }
}