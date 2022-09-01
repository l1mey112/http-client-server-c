#include "server.h"

/* 
 * 1. Create socket with socket() call
 * 2. bind() this to an IP and port where it can
 * 3. listen() for connections
 * 4. accept() connection and send() or receive() data to/from connected sockets
 */

#define SIZE 1024
#define BACKLOG 10  // Passed to listen()

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

    if (listen(server_fd, BACKLOG) < 0)
        fatal("Server failed to start listening for connections");
    
    Server server = {
        .address = server_address,
        .sock_fd = server_fd,
        .cache_position = 0,
        .cache = {0},
        .webroot.cstr = NULL,
        .route_amt = 0,
        .port = port
    };

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

// assumes string b has a / preceeding it
// printf("%s\n",
//     join_path(slit("hello/////"), 
//     slit("/argh")).cstr
// );
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
        if (strncmp(server->routes[i].route, loc.cstr, loc.len) == 0) {
            server_info(server, "Executing route '%s'", server->routes->route);

            // avoid race conditions when accessing the same data
            pthread_mutex_lock(&server->slock); 
            bool success = server->routes[i].cb(server, &resp, &headers);
            pthread_mutex_unlock(&server->slock);

            if (!success) {
                server_warn(server, "Route '%s' failed", server->routes->route);
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
            file_path = join_path(file_path, slit("index.html"));
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
        
        // Apply mimetype header
        string mimetype = match_file_type(file_path);
        builder_append_cstr(&headers, "Content-Type: ");
        builder_append(&headers, mimetype);
        builder_append_cstr(&headers, "\r\n");
    }

    builder_append_cstr(&headers, "\r\n");

    send(conn, headers.data, headers.len, 0);
    if (file) {
        send(conn, file->data, file->len, 0);
    } else {
        send(conn, resp.data, resp.len, 0);
    }
    
EXIT: // absolute lowest hanging fruit, do better next time
    close(conn);
    server_info(server, "Connection closed");

    free(resp.data);
    free(headers.data);
    string_free(&request);
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
        args->server = server;
        args->client_sock_fd = client_sock_fd;
        server_info(server, "Dispatched handler on new connection");
        pthread_create(&args->tid, NULL, (void*)dispatch, args);
        pthread_detach(args->tid);
    }
}

// UNUSED FOR NOW, UNTIL I GET PROPER PATH NORMALISATION/HANDLING
void server_serve_content(Server *server, const char *fp) {
    // TODO: make sure path is a directory, can be found
    //       and is not a file!

    server->webroot = (string){.cstr = (char *)fp, .len = strlen(fp), .is_literal = 1};
}

void server_assign_route(Server *server, const char *route, route_cb cb) {
    // TODO: check for duplicates,
    //       check for an absolute route (eg "/hello")
    //       make as dynarray
    
    server->routes[server->route_amt++] = (Route) {
        .route = route,
        .cb = cb
    };
    assert(server->route_amt < ROUTES_CAP);
}

/* 
 * curl -s -D - http://localhost:8080/strings.c -o /dev/null | bat -A
 * > mimetype text/plain
 * curl -s -D - http://localhost:8080/ -o /dev/null | bat -A
 * > mimetype text/html
 */

void new_header(str_builder *builder, const char *key, const char *value) {
    builder_append_cstr(builder, key);
    builder_append_cstr(builder, value);
    builder_append_cstr(builder, "\r\n");
}

bool debug_route(Server *server, str_builder *resp, str_builder *headers)
{
    new_header(headers, "Content-Type: ", "text/plain");

    builder_append_buf(resp, cooltext.cstr, cooltext.len);
    builder_append_cstr(resp, "Small webserver written in pure C!\n\n");
    builder_append_cstr(resp, "Routes binded: \n");
    for (int i = 0; i < server->route_amt; i++)
    {
        builder_append_cstr(resp, "    ");
        builder_append_cstr(resp, server->routes[i].route);
        // for SOME reason doing a printf with the route made the result unsusable
        builder_printf(resp, "\n");
    }
    builder_printf(resp, "Currently cached files: \n");
    for (int i = 0; i < CACHE_RING_BUFFER_LEN; i++)
    {
        if (server->cache[i].data)
            builder_printf(resp, "    %s w/ %zu bytes loaded\n", server->cache[i].path.cstr, server->cache[i].len);
    }

    return true;
}

bool route_html(Server *s, str_builder *resp, str_builder *headers)
{
    builder_append_cstr(resp, "<h1>Hello!</h1>");

    return true;
}

bool route_jpg(Server *s, str_builder *resp, str_builder *headers)
{
    new_header(headers, "Content-Type: ", "image/jpeg");

    char *ret; int64_t len;
    
    if (!read_file("among_drip.jpg", &ret, &len)) return false;

    builder_append_buf(resp, ret, len);
    free(ret);

    return true;
}

int main() {
    
    Server s;

    s = server_create(8068, INADDR_LOOPBACK); // 127.0.0.1:8080
        server_serve_content(&s, "www/");
        server_assign_route(&s, "/hello", route_html);
        server_assign_route(&s, "/among", route_jpg);
        server_assign_route(&s, "/__debug__", debug_route);
        server_run(&s);
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