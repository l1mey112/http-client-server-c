#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h> // for getnameinfo()

// Usual socket headers

#include "server.h"

#include <arpa/inet.h>

/* 
 * 1. Create socket with socket() call
 * 2. bind() this to an IP and port where it can
 * 3. listen() for connections
 * 4. accept() connection and send() or receive() data to/from connected sockets
 */

void setHttpHeader(char httpHeader[])
{
    // File object to return
    FILE *htmlData = fopen("index.html", "r");

    char line[100];
    char responseData[8000];
    while (fgets(line, 100, htmlData) != 0) {
        strcat(responseData, line);
    }
    // char httpHeader[8000] = "HTTP/1.1 200 OK\r\n\n";
    strcat(httpHeader, responseData);
}

#define SIZE 1024
#define BACKLOG 10  // Passed to listen()

const int port = 8080;

void fatal( const char* format, ... ) {
	__builtin_va_list args;
	fprintf( stderr, "FATAL: " );
	__builtin_va_start( args, format );
	vfprintf( stderr, format, args );
	__builtin_va_end( args );
	fprintf( stderr, "\n" );
    exit(1);
}
void warn( const char* format, ... ) {
	__builtin_va_list args;
	fprintf( stderr, "WARN: " );
	__builtin_va_start( args, format );
	vfprintf( stderr, format, args );
	__builtin_va_end( args );
	fprintf( stderr, "\n" );
}
void info( const char* format, ... ) {
	__builtin_va_list args;
	fprintf( stderr, "INFO: " );
	__builtin_va_start( args, format );
	vfprintf( stderr, format, args );
	__builtin_va_end( args );
	fprintf( stderr, "\n" );
}

void report(const struct sockaddr_in *serverAddress)
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

    info("Server listening on http://%s:%d", hostBuffer, port);
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

    if (bind(server_fd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
        fatal("Server failed to bind to port %d", port);

    if (listen(server_fd, BACKLOG) < 0)
        fatal("Server failed to start listening for connections");

    Server server = {
        .address = server_address,
        .sock_fd = server_fd
    };

    if (pthread_mutex_init(&server.slock, NULL) != 0)
        fatal("Mutex creation failed");

    report(&server.address);

    return server;
}

typedef struct {
    Server *server;
    int client_sock_fd;
    pthread_t tid;
} dispatch_args;

void dispatch(dispatch_args *args) {
    Server *server = args->server;
    
    info("Dispatched handler on new connection");
    send(args->client_sock_fd, H_HTTP_200, strlen(H_HTTP_200), 0);

    send(args->client_sock_fd, server->data_to_serve, server->data_len, 0);
    
    close(args->client_sock_fd);
    info("Connection over");
}

void run(Server *server) {
    for(;;) {
        int client_sock_fd = accept(server->sock_fd, NULL, NULL);
        if (client_sock_fd < 0) {
            warn("Failed to accept a connection");
            continue;
        }
        dispatch_args *args = malloc(sizeof(dispatch_args));
        args->server = server;
        args->client_sock_fd = client_sock_fd;
        pthread_create(&args->tid, NULL, (void*)dispatch, args);
        pthread_detach(args->tid);
    }
}

void serve_file(Server *server, const char *filepath){
	FILE *fp = fopen(filepath, "r");
	assert(fp != NULL && "failed to open file!");
	assert(fseek(fp, 0, SEEK_END) == 0);
	
	int64_t len = ftell(fp);
	assert(len > 0);

	rewind(fp);

	uint8_t *buffer = malloc(len);
	assert(buffer);
	size_t elms = fread(buffer, 1, len, fp);

	fclose(fp);

	if (elms == 0 && len > 0) {
		assert(0);
	}

	assert(elms != 0 && "cannot read empty file!");

	server->data_to_serve = buffer;
    server->data_len = len;
}

int main()
{
    Server server = create_server(8080, INADDR_LOOPBACK);
    serve_file(&server, "index.html");
    run(&server);
}