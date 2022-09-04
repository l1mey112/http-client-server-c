#ifndef _H_SERVER
#define _H_SERVER

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
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
#include <signal.h>
#include <ctype.h>
#include <dirent.h>

#include "strings.h"

#define CACHE_RING_BUFFER_LEN 4

typedef struct {
    string path;
    char *data;
    int64_t len;
} CachedFile;

#define ROUTES_CAP 12

typedef struct Server Server;

typedef bool (*route_cb)(Server *server, str_builder *resp, str_builder *headers);

typedef struct {
    string route;
    route_cb cb;
} Route;

#define DEFINE_ROUTE(name) \
    bool name(Server *server, str_builder *resp, str_builder *headers)

// big difference between #define(f) and #define (f)

struct Server {
    struct sockaddr_in address;
    int sock_fd;
    int port;

    pthread_mutex_t log_lock; 
    // so logging on different threads don't mesh together

    pthread_mutex_t slock;
    CachedFile cache[CACHE_RING_BUFFER_LEN];
    int cache_position;

    Route routes[ROUTES_CAP]; // should be a dynarray later
    int route_amt;

    string webroot; // is NULL initially, to not serve static files
};

void fatal( const char* format, ...);
void server_fatal( Server *server, const char* format, ...);
void server_warn( Server *server, const char* format, ...);
void server_info( Server *server, const char* format, ... );
void server_errno( Server *server, const char* format, ... );

CachedFile *server_get_file(Server *server, string file_path);
bool read_file(const char *filepath, char **ret, int64_t *len_en);

#define mimetypes_len 116
extern const string mimetypes[];
string match_file_type(string path);


// ------ ROUTES ------

DEFINE_ROUTE(debug_route);
DEFINE_ROUTE(flush_caches_route);
DEFINE_ROUTE(route_html);
DEFINE_ROUTE(route_jpg);
DEFINE_ROUTE(ls_route);
DEFINE_ROUTE(fail_route);

#endif