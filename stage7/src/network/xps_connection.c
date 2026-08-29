#include "xps_connection.h"

void strrev(char *str) {
	for (int l = 0, r = strlen(str) - 2; l < r; l++, r--) {
		char temp = str[l];
		str[l] = str[r];
		str[r] = temp;
	}
}

void connection_loop_read_handler(void *ptr);

// takes in epoll fd and sock fd returns a connection instance
// step 1 : dynamically allocate the connection instance
// step 2 : attach the connection fd to the epoll fd using xps_loop_attach
// step 3 : fill in the instance values
// step 4 : add the connection in global connections vector
xps_connection_t *xps_connection_create(xps_core_t *core, u_int sock_fd) {

    xps_connection_t *connection = malloc(sizeof(xps_connection_t));
    if (connection == NULL) {
        logger(LOG_ERROR, "xps_connection_create()", "malloc() failed for 'connection'");
        return NULL;
    }

    /* attach sock_fd to epoll */
    xps_loop_attach(core->loop, sock_fd, EPOLLIN, connection, connection_loop_read_handler);

    // Init values
    connection->core = core;
    connection->sock_fd = sock_fd;
    connection->listener = NULL;
    connection->remote_ip = get_remote_ip(sock_fd);

    /* add connection to 'connections' list */
    vec_push(&core->connections, connection);

    logger(LOG_DEBUG, "xps_connection_create()", "created connection");
    return connection;
}

// takes in connection instance
// step 1 : validate the connection instance
// step 2 : find the corresponding connection from global connections array and make it NULL,
// similar to what we did in listeners global array step 3 : detach the connection socket from epoll
// interest list step 4 : close the connection socket step 5 : deallocate memory for the string
// remote ip in connection instance step 6 : free connection instance
void xps_connection_destroy(xps_connection_t *connection) {
    /* validate params */
    assert(connection != NULL);
    xps_core_t *core = connection->core;
        /* set connection to NULL in 'connections' list */
        for (int i = 0; i < core->connections.length; i++) {
        xps_connection_t *curr = core->connections.data[i];
        if (curr == connection) {
            core->connections.data[i] = NULL;
            break;
        }
    }

    /* detach connection from loop */
    xps_loop_detach(core->loop, connection->sock_fd);

    /* close connection socket FD */
    close(connection->sock_fd);

    /* free connection->remote_ip */
    free(connection->remote_ip);

    /* free connection instance */
    free(connection);

    logger(LOG_DEBUG, "xps_connection_destroy()", "destroyed connection");
}

// takes in connection instance
// used to handle epoll ready list connection request
// step 1 : validate the instance
// step 2 : create a buffer to store the input
// step 3 : recv the value into the buffer
// step 4 : print client message
// step 5 : reverse it 
// step 6 : send it back to the client
void connection_loop_read_handler(void *ptr) {
    /* validate params */
    assert(ptr != NULL);
    xps_connection_t *connection = ptr;

    char buff[DEFAULT_BUFFER_SIZE];
    memset(buff, 0, DEFAULT_BUFFER_SIZE);
    long read_n = recv(connection->sock_fd, buff, DEFAULT_BUFFER_SIZE, 0); /* read data from client using recv() */

        if (read_n < 0) {
        logger(LOG_ERROR, "xps_connection_read_handler()", "recv() failed");
        perror("Error message");
        xps_connection_destroy(connection);
        return;
    }

    if (read_n == 0) {
        logger(LOG_INFO, "connection_read_handler()", "peer closed connection");
        xps_connection_destroy(connection);
        return;
    }

    buff[read_n] = '\0';

    /* print client message */
    printf("[CLIENT MESSAGE] %s\n", buff);

    /* reverse client message */
    strrev(buff);


    // Sending reversed message to client
    long bytes_written = 0;
    long message_len = read_n;
    while (bytes_written < message_len) {
        long write_n = send(connection->sock_fd, buff + bytes_written, message_len - bytes_written, 0);/* send message using send() */
            if (write_n < 0) {
            logger(LOG_ERROR, "xps_connection_read_handler()", "send() failed");
            perror("Error message");
            xps_connection_destroy(connection);
            return;
        }
        bytes_written += write_n;
    }
}