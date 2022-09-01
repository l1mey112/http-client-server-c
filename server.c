#include "server.h"

/* 
 * 1. Create socket with socket() call
 * 2. bind() this to an IP and port where it can
 * 3. listen() for connections
 * 4. accept() connection and send() or receive() data to/from connected sockets
 */

#define SIZE 1024
#define BACKLOG 10  // Passed to listen()

const int port = 8080;

string cooltext = slit(
" _____    _     __    __          __ _  __ __ _ \\    / __ _ \n"
"(_  | |V||_)|  |_    /     \\    /|_ |_)(_ |_ |_) \\  / |_ |_)\n"
"__)_|_| ||  |__|__   \\__    \\/\\/ |__|_)__)|__| \\  \\/  |__| \\\n\n");
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

    server_info(server, "Server listening on http://%s:%d", hostBuffer, port);
}

Server create_server(int port, uint32_t bind_addr){
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

    if (listen(server_fd, BACKLOG) < 0)
        fatal("Server failed to start listening for connections");
    
    Server server = {
        .address = server_address,
        .sock_fd = server_fd,
        .cache_position = 0,
        .cache = {0}
    };

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

void RET_404(Server *server, int conn){
    send(conn, H_HTTP_404, strlen(H_HTTP_404), 0);
    send(conn, M_HTTP_404, strlen(M_HTTP_404), 0);
    close(conn);
    server_info(server, "Connection closed");
}

void new_key_value(str_builder *builder, const char *key, string value){
    builder_append_cstr(builder, key);
    builder_append(builder, value);
    builder_append_cstr(builder, "\r\n");
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
    string request = string_from_buf(buf, sz);

    string req_type = string_pop_delimit(&request, slit(" "));
    if (string_is_strerr(req_type) || !string_equals(req_type, slit("GET"))) {
        server_warn(server, "Request is not GET");
        RET_404(server, conn);
        return;
    }

    string loc = string_pop_delimit(&request, slit(" "));
    if (string_is_strerr(req_type)) {
        server_warn(server, "Malformed request");
        RET_404(server, conn);
        return;
    }

    str_builder resp = builder_make(64);
    builder_append_cstr(&resp, H_HTTP_200);

    CachedFile tmp;
    CachedFile *read_f = &tmp;
    if (string_equals(loc, slit("/__debug__"))) {
        read_f->path = slit("/__debug__");
        str_builder b = builder_make(cooltext.len);
        builder_append_buf(&b, cooltext.cstr, cooltext.len);
        
        builder_printf(&b, "Small webserver written in pure C!\n\n");
        builder_printf(&b, "Currently cached files: \n");
        for (int i = 0; i < CACHE_RING_BUFFER_LEN; i++)
        {
            builder_printf(&b, "    %s w/ %zu bytes loaded\n", server->cache[i].path, server->cache[i].len);
        }

        builder_printf(&b, "\nThere isn't much debug information anyway...\n");

        read_f->data = b.data;
        read_f->len = b.len;
    } else {
        string file_path;
        if (string_equals(loc, slit("/"))) {
            file_path = slit("index.html");
        } else {
            if (loc.cstr[0] != '/') {
                server_warn(server, "Malformed location request");
                RET_404(server, conn);
                free(resp.data);
                return;
            }

            file_path = string_substr(loc,1,loc.len);
        }
        
        read_f = server_get_file(server, file_path);

        if (read_f == NULL){
            server_warn(server, "Could not find file, requested %s",file_path.cstr);
            RET_404(server, conn);
            free(resp.data);
            return;
        }

        server_info(server, "Requested '%s', serving '%s'",loc.cstr, file_path.cstr);
        
        string mimetype = match_file_type(file_path);
        new_key_value(&resp, "content-type: ", mimetype);
    }
    
    
    // Start creating response
    builder_append_cstr(&resp, "\r\n");
    send(conn, resp.data, resp.len, 0);
    send(conn, read_f->data, read_f->len, 0);
    free(resp.data);

    close(conn);
    server_info(server, "Connection closed");
}

void run(Server *server) {
    for(;;) {
        int client_sock_fd = accept(server->sock_fd, NULL, NULL);
        if (client_sock_fd < 0) {
            server_warn(server, "Failed to accept a connection");
            continue;
        }
        dispatch_args *args = malloc(sizeof(dispatch_args));
        args->server = server;
        args->client_sock_fd = client_sock_fd;
        server_info(server, "Dispatched handler on new connection");
        pthread_create(&args->tid, NULL, (void*)dispatch, args);
        pthread_detach(args->tid);
    }
}

/* 
 * curl -s -D - http://localhost:8080/strings.c -o /dev/null | bat -A
 * > mimetype text/plain
 * curl -s -D - http://localhost:8080/ -o /dev/null | bat -A
 * > mimetype text/html
 */

int main()
{
    Server server = create_server(8080, INADDR_LOOPBACK);
    run(&server);
}

/*
 * echo "1" | doas tee /proc/sys/net/ipv4/tcp_tw_reuse 
 */

/* 
 * TODO:
 *   route_add(server,"/debug",server_cb);
 *   route_add(server,"/test/a",server_cb);
 *   static_serve(server, ".");
 */

/* 
 * inside the route adding callback
 * pass in a string builder
 * and respond with the string builder's contents
 */

/* 
 * TODO:
 *   use vnsprintf or whatever to get a length,
 *   then use that inside a string builder to reserve 
 *   chars and use a printf implementation that you 
 *   can append with
 */

const char * H_HTTP_200 = "HTTP/1.1 200 OK\r\n";
const char * H_HTTP_404 = "HTTP/1.0 404 Not Found\r\n\r\n";
const char * M_HTTP_404 = "<h1>404 not found!</h1>";