#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#define PORT 8080
#define BUFF_SIZE 10000

typedef struct {
    char message[BUFF_SIZE];
    struct sockaddr_in client_addr;
    int sockfd;
    socklen_t addr_len;
} client_data_t;

void strrev(char *str){
    for(int start=0, end=strlen(str) - 2; start<end ; start++, end--){
        char temp = str[start];
        str[start] = str[end];
        str[end]=temp;
    }
}

void* handle_client(void* arg) {
    // type cast to our struct
    client_data_t* data = (client_data_t*)arg;
    printf("[CLIENT_MESSAGE] %s\n", data->message);
    // printf(
    //     "[THREAD %lu]\n"
    //     "Message      : %s"
    //     "Client IP    : %s\n"
    //     "Client Port  : %d\n"
    //     "Socket FD    : %d\n"
    //     "Addr Length  : %u\n",
    //     (unsigned long)pthread_self(),
    //     data->message,
    //     inet_ntoa(data->client_addr.sin_addr),
    //     ntohs(data->client_addr.sin_port),
    //     data->sockfd,
    //     data->addr_len
    // );

    // reverse the message
    strrev(data->message);

    // send back to the server
    sendto(data->sockfd, data->message, strlen(data->message), 0, (struct sockaddr*)&(data->client_addr), data->addr_len);

    free(data);
    pthread_exit(NULL);
}

int main () {
    char buffer[BUFF_SIZE];
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0); // create a UDP socket
    pthread_t thread_id;

    struct sockaddr_in server_addr, client_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)); // bind it to ip, port

    printf("[INFO] server listening on port %d\n", PORT);

    while(1) {
        // value - result parameter
        socklen_t len = sizeof(client_addr);
        // take in whatever into the user buffer and also saves the client detail in client_addr
        ssize_t n = recvfrom(sockfd, buffer, BUFF_SIZE, 0, (struct sockaddr*)&client_addr, &len);
        buffer[n] = '\0';

        client_data_t* data = (client_data_t*)malloc(sizeof(client_data_t));
        strcpy(data->message, buffer);
        data->client_addr = client_addr;
        data->sockfd = sockfd;
        data->addr_len = len;

        // create a new thread to handle this client
        if (pthread_create(&thread_id, NULL, handle_client, (void*)data) != 0) {
            perror("Failed to create thread");
            free(data);
        }

        // detach the thread to allow independent execution
        pthread_detach(thread_id);
    }

    close(sockfd);
    return 0;
}