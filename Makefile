override CFILES := $(shell find ./ -type f -name '*.c' -not -wholename '*example*')
override HFILES := $(shell find ./ -type f -name '*.h' -not -wholename '*example*')

.PHONY: run
run: a.out
	@./a.out

a.out: $(CFILES) $(HFILES) Makefile
	@gcc -static-libasan -Wall -ggdb $(CFILES)
#	-fsanitize=address,pointer-compare,leak,undefined,pointer-overflow -fanalyzer
#	@tcc -Wall -rdynamic -fno-inline -g -bt25 -ggdb $(CFILES)