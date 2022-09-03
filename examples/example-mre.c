#include "strings.h"

/* "mre" - minimal reproducable example.
   this program was used to debug builder_printf() */

int main(){
	str_builder b = builder_make(10);

	builder_printf(&b, "hello %s, %d, pointer is %p\n", "world", 6, NULL);

	fwrite(b.data, 1, b.len, stdout);
}