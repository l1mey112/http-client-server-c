#ifndef _H_DYNAMIC_STRINGS
#define _H_DYNAMIC_STRINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

//#define MSG(a, args...) fprintf(stderr, "[%s:%d: %s]: \x1b[92m"a"\x1b[39m\n",__FILE__,__LINE__,__func__,args)
#define MSG(a, args...)

typedef struct {
	char *data;
	uint32_t len;
	uint32_t cap;
} str_builder;

typedef struct {
	char *cstr;
	uint32_t len;
	int is_literal;
} string;

#define slit(s) (string){(char *)s, sizeof(s)-1, 1}
#define strerr (string){(char *)0, 0, 0}

void string_free(string *a);
string string_clone(string a);
string string_concat(string a, string b);
bool string_contains(string str, string substr);
bool string_equals(string a, string b);
string string_trim_all_whitespace(string a);
string string_pop_delimit(string *str, string substr);
bool string_is_strerr(string a);
string string_from_buf(char *buf, size_t len);
string string_substr(string a, int start, int end);

str_builder builder_make(uint32_t size);
void builder_ensure_cap(str_builder *builder, uint32_t len);
void builder_append(str_builder *builder, string a);
void builder_append_cstr(str_builder *builder, const char *a);
void builder_append_buf(str_builder *builder, char *a, size_t len);
string builder_tostr(str_builder *builder);
void builder_printf(str_builder *builder, const char* format, ...);

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

#endif