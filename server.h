#ifndef _H_SERVER
#define _H_SERVER

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdbool.h>

#include "strings.h"

extern const char * H_HTTP_200;
extern const char * H_HTTP_404;
extern const char * M_HTTP_404;

#define CACHE_RING_BUFFER_LEN 4

typedef struct {
    string path;
    char *data;
    int64_t len;
} CachedFile;

typedef struct {
    struct sockaddr_in address;
    int sock_fd;

    pthread_mutex_t slock;
    CachedFile cache[CACHE_RING_BUFFER_LEN];
    int cache_position;
} Server;

void fatal( const char* format, ...);
void server_fatal( Server *server, const char* format, ...);
void server_warn( Server *server, const char* format, ...);
void server_info( Server *server, const char* format, ... );

CachedFile *server_get_file(Server *server, string file_path);
bool read_file(const char *filepath, char **ret, int64_t *len_en);

#define mimetypes_len 116
extern const string mimetypes[];
string match_file_type(string path);

#endif