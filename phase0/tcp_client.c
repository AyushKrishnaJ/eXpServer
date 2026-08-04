#include <arpa/inet.h> // htons, htonl, inet_ntoa, etc. — byte-order & address conversion
#include <netdb.h>
#include <netinet/in.h> // struct sockaddr_in
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h> // socket, bind, listen, accept, send, recv, setsockopt
#include <unistd.h>

#define SERVER_PORT 8080
#define BUFF_SIZE 10000

int main() {
    // create a client socket
    int client_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock_fd < 0) {
        perror("[ERROR] socket() failed");
        exit(1);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(SERVER_PORT);

    if (connect(client_sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        printf("[ERROR] Failed to connect to tcp server\n");
        close(client_sock_fd);
        exit(1);
    } else {
        printf("[INFO] Connected to tcp server\n");
    }

    // read a line from terminal
    // send it over to the server
    // receive the response from the server
    // print the response to the terminal
    char *line;
    ssize_t line_len = 0;
    while(1) {
        // step 1:
        ssize_t read_n = getline(&line, &line_len, stdin);

        if (read_n < 0) {
            printf("[ERROR] Failed to read line from stdin\n");
            free(line);
            close(client_sock_fd);
            exit(1);
        }

        // step 2:
        send(client_sock_fd, line, read_n, 0);

        char buff[BUFF_SIZE];
        memset(buff, 0, BUFF_SIZE);

        // step 3:
        read_n = recv(client_sock_fd, buff, BUFF_SIZE, 0);

        if (read_n <= 0) {
            if (read_n < 0) {
                printf("[ERROR] Failed to receive data from server\n");
            }

            else if (read_n == 0) {
                printf("[INFO] Connection closed by server\n");
            }

            free(line);
            close(client_sock_fd);
            exit(1);            
        }

        // step 4:
        printf("[SERVER MESSAGE] %s", buff);
    }
    
    free(line);
    return 0;
}