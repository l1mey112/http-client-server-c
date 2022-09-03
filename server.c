#include "server.h"

void report(Server *server, const struct sockaddr_in *serverAddress)
{
    char hostBuffer[INET6_ADDRSTRLEN];
    char serviceBuffer[NI_MAXSERV]; // defined in `<netdb.h>`
    socklen_t addr_len = sizeof(*serverAddress);
    int err = getnameinfo(
        (struct sockaddr *) serverAddress,
        addr_len,
        hostBuffer,
        sizeof(hostBuffer),
        serviceBuffer,
        sizeof(serviceBuffer),
        NI_NUMERICHOST
    );
    if (err != 0)
        fatal("Failed to host information");

    server_info(server, "Server listening on http://%s:%d", hostBuffer, server->port);
}

Server server_create(int port, uint32_t bind_addr){
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    const struct sockaddr_in server_address = (struct sockaddr_in){
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = htonl(bind_addr)
    };
    //  INADDR_LOOPBACK = 127.0.0.1
    //  INADDR_ANY      =   0.0.0.0

    const int a = 0;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &a, sizeof(a)) < 0 ||
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &a, sizeof(a)) < 0 )
        fatal("Server failed to set socket options");

    if (bind(server_fd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
        fatal("Server failed to bind to port %d", port);

    if (listen(server_fd, 10) < 0)
        fatal("Server failed to start listening for connections");
    
    Server server = {
        .address = server_address,
        .sock_fd = server_fd,
        .cache_position = 0,
        .webroot.cstr = NULL,
        .route_amt = 0,
        .port = port
    };
    memset(&server.cache,0, sizeof(server.cache));

    if (pthread_mutex_init(&server.log_lock, NULL) != 0)
        fatal("Mutex creation failed");
    if (pthread_mutex_init(&server.slock, NULL) != 0)
        fatal("Mutex creation failed");


    report(&server, &server.address);

    return server;
}

typedef struct {
    Server *server;
    int client_sock_fd;
    pthread_t tid;
} dispatch_args;

const char * H_HTTP_404 = "HTTP/1.0 404 Not Found\r\n\r\n";
const char * M_HTTP_404 = "<h1>404 Not Found</h1>";

const char * H_HTTP_400 = "HTTP/1.0 400 Bad Request\r\n\r\n";
const char * M_HTTP_400 = "<h1>400 Bad Request</h1>";

const char * H_HTTP_500 = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
const char * M_HTTP_500 = "<h1>500 Internal Server Error</h1>";

#define SEND_404 send(conn, H_HTTP_404, strlen(H_HTTP_404), 0); send(conn, M_HTTP_404, strlen(M_HTTP_404), 0)
#define SEND_400 send(conn, H_HTTP_400, strlen(H_HTTP_400), 0); send(conn, M_HTTP_400, strlen(M_HTTP_400), 0)
#define SEND_500 send(conn, H_HTTP_500, strlen(H_HTTP_500), 0); send(conn, M_HTTP_500, strlen(M_HTTP_500), 0)

string join_path(string a, string b){
    int right = a.len;
	for (char *ptr = a.cstr + a.len - 1; *ptr == '/'; ptr--)
		right--;
    
    a.len = right;
    return string_concat(a, b);
}

void dispatch(dispatch_args *args) {
    int conn = args->client_sock_fd;
    Server *server = args->server;

    char buf[1024];
    size_t sz = recv(conn, buf, 1024, 0);
    if (sz == 0 || sz == -1) {
        close(conn);
        return;
    }

    // Allocate
    string request = string_from_buf(buf, sz);
    str_builder headers = builder_make(40);
    str_builder resp = builder_make(120);

    string req_type = string_pop_delimit(&request, slit(" "));
    if (string_is_strerr(req_type) || !string_equals(req_type, slit("GET"))) {
        server_warn(server, "Request is not GET");
        SEND_400;
        goto EXIT;
    }

    string loc = string_pop_delimit(&request, slit(" "));
    if (string_is_strerr(req_type)) {
        server_warn(server, "Malformed request");
        SEND_400;
        goto EXIT;
    }

    builder_append_cstr(&headers, "HTTP/1.1 200 OK\r\n");
    builder_append_cstr(&headers, "Server: Simple-C-Server\r\n");

    bool found_route = false;
    for (int i = 0; i < server->route_amt; i++)
    {
        if (string_equals(server->routes[i].route, loc)) {
            server_info(server, "Executing route '%s'", server->routes[i].route);

            // avoid race conditions when accessing the same data
            pthread_mutex_lock(&server->slock); 
            bool success = server->routes[i].cb(server, &resp, &headers);
            pthread_mutex_unlock(&server->slock);

            if (!success) {
                server_warn(server, "Route '%s' failed", server->routes[i].route);
                SEND_500;
                goto EXIT;
            }
            
            found_route = true;
            break;
        }
    }

    if (!found_route && string_is_strerr(server->webroot)){
        SEND_404;
        goto EXIT;
    }

    CachedFile *file = NULL;
    if (!found_route) {
        string file_path = server->webroot;
        if (string_equals(loc, slit("/"))) {
            file_path = join_path(file_path, slit("/index.html"));
        } else {
            if (loc.cstr[0] != '/') {
                server_warn(server, "Malformed location request");
                SEND_404;
                
                goto EXIT;
            }

            file_path = join_path(file_path, loc);
        }
        file = server_get_file(server, file_path);

        if (file == NULL){
            server_warn(server, "Could not find file %s",file_path.cstr);
            SEND_404;

            goto EXIT;
        }

        server_info(server, "Requested '%s', serving '%s'",loc.cstr, file_path.cstr);
        // Maybe cache mimetype with file?

        // Apply mimetype header
        string mimetype = match_file_type(file_path);
        // Mimetype not found, search file for mimetype
        if (string_is_strerr(mimetype))
        {
            mimetype = slit("text/plain");
            // Search the first 50 chars for non printable characters
            for (int i = 0; i < MIN(50,file->len); i++) {
                if (!(isprint(file->data[i]) || isspace(file->data[i]))){
                    mimetype = slit("application/octet-stream");
                    break;
                }
            }
        }

        string_free(&file_path);

        builder_append_cstr(&headers, "Content-Type: ");
        builder_append(&headers, mimetype);
        builder_append_cstr(&headers, "\r\n");
    }
    builder_append_cstr(&headers, "\r\n");

    bool failed = false;

    failed |= send(conn, headers.data, headers.len, 0) == -1;
    if (file) {
        failed |= send(conn, file->data, file->len, 0) == -1;
    } else {
        failed |= send(conn, resp.data, resp.len, 0) == -1;
    }
    if (failed) {
        server_errno(server, "Sending to socket failed");
    }

//  this may sound like a cope, but anyway...
//  
//  most horrible C programmers who use goto are doing it in 
//  replacement of control flow keywords like if, for and while.
//  still, the goto keyword is useful as a break statement that 
//  exits multiple loops, or as a way of concentrating cleanup code 
//  in a single place in a function even when there are multiple 
//  ways to terminate the function. this is exactly what i am using 
//  the goto keyword for.
//  
//  sure, goto should be "avoided at all costs!!" when you are just
//  getting to learn C or C++, there are better options than a goto.
//  
//  it still has it's place in C if you know where it's applicable
EXIT: 
    close(conn);
    server_info(server, "Connection closed");

    free(resp.data);
    free(headers.data);
    string_free(&request);
    string_free(&req_type);
    string_free(&loc);
    free(args);
}

void server_run(Server *server) {
    if (string_is_strerr(server->webroot)) {
        server_warn(server, "Server is configured to not serve any local content");
    } else {
        server_info(server, "Serving content from '%s'", server->webroot.cstr);
    }

    for(;;) {
        int client_sock_fd = accept(server->sock_fd, NULL, NULL);
        if (client_sock_fd < 0) {
            server_warn(server, "Failed to accept a connection");
            continue;
        }
        dispatch_args *args = malloc(sizeof(dispatch_args));
        assert(args);
        args->server = server;
        args->client_sock_fd = client_sock_fd;
        server_info(server, "Dispatched handler on new connection");
        pthread_create(&args->tid, NULL, (void*)dispatch, args);
        pthread_detach(args->tid);
    }
}

void server_serve_content(Server *server, const char *fp) {
    struct stat path_stat;
    lstat(fp, &path_stat);
    if (!S_ISDIR(path_stat.st_mode)) {
        server_fatal(server, "Path '%s' is not a directory and so cannot be the webroot", fp);
    }
    if (access(fp, R_OK) < 0){
        server_fatal(server, "Webroot at path '%s' cannot be read", fp);
    }

    server->webroot = (string){.cstr = (char *)fp, .len = strlen(fp), .is_literal = 1};
}

void server_assign_route(Server *server, const char *route, route_cb cb) {
    size_t strl = strlen(route);
    if (!(strl && route[0] == '/')) {
        server_fatal(server, "Path '%s' is not an absolute path. Prepend it with a '/'", route);
    }

    string str_route = (string){(char *)route, (strl), ((int)1)};

    for (int i = 0; i < server->route_amt; i++)
    {
        if (string_equals(str_route, server->routes[i].route)) {
            server_fatal(server, "Duplicate route '%s'", route);
        }
    }
    
    server->routes[server->route_amt++] = (Route) {
        .route = str_route,
        .cb = cb
    };

    assert(server->route_amt < ROUTES_CAP);
    // I mean, you won't really have more than 12 routes, making it a
    // dynarray is easy though. If you want to do it, go ahead.
}

#ifdef __TINYC__
// -bt10 for backtraces
int tcc_backtrace(const char *fmt, ...);

void segfault_handler(int sig) {
    fputs("sig 11 - segmentation fault\n",stderr);
    tcc_backtrace("Backtrace");
    exit(139);
}
#endif

int main() {
#ifdef __TINYC__
    signal(11, segfault_handler);
#endif
    Server s;

    s = server_create(8080, INADDR_LOOPBACK); // 127.0.0.1:8080
        server_serve_content(&s, "www");
        server_assign_route(&s, "/hello", route_html);
        server_assign_route(&s, "/among", route_jpg);
        server_assign_route(&s, "/__debug__", debug_route);
        server_assign_route(&s, "/__flush_caches__", flush_caches_route);
        server_assign_route(&s, "/__ls__", ls_route);
        server_run(&s);
}