#include "server.h"

void fatal( const char* format, ...) {
	__builtin_va_list args;
	fprintf( stderr, "FATAL: " );
	__builtin_va_start( args, format );
	vfprintf( stderr, format, args );
	__builtin_va_end( args );
	fprintf( stderr, "\n" );
	exit(1);
}
void server_fatal( Server *server, const char* format, ...) {
	pthread_mutex_lock(&server->log_lock);
	__builtin_va_list args;
	fprintf( stderr, "FATAL: " );
	__builtin_va_start( args, format );
	vfprintf( stderr, format, args );
	__builtin_va_end( args );
	fprintf( stderr, "\n" );
	pthread_mutex_unlock(&server->log_lock);
    exit(1);
}
void server_warn( Server *server, const char* format, ... ) {
	pthread_mutex_lock(&server->log_lock);
	__builtin_va_list args;
	fprintf( stderr, "WARN: " );
	__builtin_va_start( args, format );
	vfprintf( stderr, format, args );
	__builtin_va_end( args );
	fprintf( stderr, "\n" );
	pthread_mutex_unlock(&server->log_lock);
}
void server_info( Server *server, const char* format, ... ) {
	pthread_mutex_lock(&server->log_lock);
	__builtin_va_list args;
	fprintf( stderr, "INFO: " );
	__builtin_va_start( args, format );
	vfprintf( stderr, format, args );
	__builtin_va_end( args );
	fprintf( stderr, "\n" );
	pthread_mutex_unlock(&server->log_lock);
}