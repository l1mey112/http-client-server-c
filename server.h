#ifndef _H_SERVER
#define _H_SERVER

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <assert.h>

const char * H_HTTP_200 = "HTTP/1.1 200 OK\r\n\n";

typedef struct {
    pthread_mutex_t slock;

    char *data_to_serve;
    size_t data_len;

    struct sockaddr_in address;
    int sock_fd;
} Server;

#endif