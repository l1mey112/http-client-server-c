#include "strings.h"

int main(){
	string hello = slit("31/08/2022");
    printf("%s\n", string_pop_delimit(&hello, slit("/")).cstr );
    printf("%s\n", string_pop_delimit(&hello, slit("/")).cstr );
    printf("%s\n", hello.cstr);
}