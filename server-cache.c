#include "server.h"

CachedFile *server_get_file(Server *server, string file_path){
	
	// File in cache?
	for (int i = 0; i < CACHE_RING_BUFFER_LEN; i++)
	{
		if (string_equals(server->cache[i].path, file_path)) {
			server_info(server, "Found file '%s' in cache", file_path.cstr);
			return &server->cache[i];
		}
	}
	
	pthread_mutex_lock(&server->slock);
	// No file in cache, read it and insert into ring buffer
	if (server->cache[server->cache_position].data){
		free(server->cache[server->cache_position].data);
		string_free(&server->cache[server->cache_position].path);
	}

	if (!read_file(file_path.cstr, 
                   &server->cache[server->cache_position].data, 
		           &server->cache[server->cache_position].len)
	) {
		server->cache[server->cache_position].data = NULL;
		server->cache[server->cache_position].len = 0;
		pthread_mutex_unlock(&server->slock);
		return NULL;
	}

	server->cache[server->cache_position].path = file_path;
	
	int pos = server->cache_position;

	// Circle buffer wrap
	server->cache_position++;
	server->cache_position %= CACHE_RING_BUFFER_LEN;

	pthread_mutex_unlock(&server->slock);
	server_info(server, "Read and cached file '%s'",file_path.cstr);
	return &server->cache[pos];
}

bool read_file(const char *filepath, char **ret, int64_t *len_en){
	FILE *fp = fopen(filepath, "r");
    if (fp == NULL ) return false;
	if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return false;
    }
	
	int64_t len = ftell(fp);
	assert(len > 0);

	rewind(fp);

	char *buffer = malloc(len);
	assert(buffer);
	size_t elms = fread(buffer, 1, len, fp);

	fclose(fp);

	if (elms == 0 || ( elms == 0 && len > 0 )) {
		free(buffer);
        return false;
	}

    *len_en = len;
	*ret = buffer;
    return true;
}