#include "strings.h"

str_builder builder_make(uint32_t size){
	MSG("created str builder with size %u",size);
	return (str_builder){
		.data = malloc(size),
		.len  = 0,
		.cap  = size
	};
}

void builder_ensure_cap(str_builder *builder, uint32_t len){
	if (builder->len + len > builder->cap) {
		builder->cap *= 2;
		if (builder->cap < builder->len + len)
			builder->cap = builder->len + len;
		builder->data = realloc(builder->data, builder->cap);
		MSG("additional %u bytes (new cap: %u)",len, builder->cap);
	}
}

void builder_append(str_builder *builder, string a){
	MSG("appending str (old len: %u, new len: %u)", builder->len, builder->len + a.len);
	builder_ensure_cap(builder, a.len);
	memcpy(builder->data + builder->len, a.cstr, a.len);
	builder->len += a.len;
}

void builder_append_cstr(str_builder *builder, const char *a){
	MSG("appending cstr (old len: %u, new len: %u)", builder->len, builder->len + a.len);
	uint32_t len = strlen(a);
	builder_ensure_cap(builder, len);
	memcpy(builder->data + builder->len, a, len);
	builder->len += len;
}

void builder_append_buf(str_builder *builder, char *a, size_t len){
	MSG("appending buf (old len: %u, new len: %u)", builder->len, builder->len + a.len);
	builder_ensure_cap(builder, len);
	memcpy(builder->data + builder->len, a, len);
	builder->len += len;
}

void builder_printf(str_builder *builder, const char* format, ...){
	__builtin_va_list args;
	__builtin_va_start (args, format);
	int max_size = snprintf(NULL, 0, format, args); 
	// snprintf returns max size you might need, sprintf returns amount actually written
	builder_ensure_cap(builder, max_size + 1);
	// reserve space for \0, avoids a buffer overflow. we don't use it anyway
	int size = sprintf(builder->data + builder->len, format, args);
	// yeah uhh do not pass in max_size to sprintf, it will chop off the last char and put a \0
	__builtin_va_end (args);
	builder->len += size;
}

string builder_tostr(str_builder *builder){
	string ret = {0};
	ret.len  = builder->len;
	ret.cstr = malloc(builder->len + 1);
	assert(ret.cstr);
	ret.cstr[builder->len] = 0;
	
	memcpy(ret.cstr, builder->data, builder->len);
	free(builder->data);
	
	MSG("returned a string from a builder, freed builder",0);
	return ret;
}