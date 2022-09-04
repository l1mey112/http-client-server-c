#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char *initial_line = "HTTP/1.0 200 OK\r\n\r\n";
const char *response = "<h1>Hello world!</h1>";

int main()
{
    int port = 8080;
    uint32_t bind_addr = INADDR_LOOPBACK;
//  INADDR_LOOPBACK is just the loopback address, 'localhost'
//  INADDR_ANY would be 0.0.0.0, exposing it to the outside
//	http://127.0.0.1:8080/

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
//	Open a socket as a file descriptor

    struct sockaddr_in server_address;
//	Struct to store the server address and bind port for the socket

    server_address.sin_family = AF_INET;               // IP protocall family
    server_address.sin_port = htons(port);             // Port number
    server_address.sin_addr.s_addr = htonl(bind_addr); // Bind address

    struct sockaddr *bindptr = (struct sockaddr *)&server_address;

    if (
		bind(server_fd, bindptr, sizeof(server_address)) < 0 ||
		listen(server_fd, 10) < 0
	) abort();
//	Bind socket using struct information and start listening, abort on fail

//	Buffer stores entire request data, GET requests are rarely this big
    char buf[1024];

//	Start event loop
//	Wait for a new connection on the socket and return it's file descriptor
    for(;;) {
        int client = accept(server_fd, NULL, NULL);

//		Read from the client into the buffer and print the request
        size_t sz = recv(client, buf, 1024, 0);
        fwrite(buf, 1, sz, stdout);

//		Send the initial line and the html data to satisfy the GET request
        send(client, initial_line, strlen(initial_line), 0);
        send(client, response, strlen(response), 0);
//		Close the connection with the client
        close(client);
    }
}