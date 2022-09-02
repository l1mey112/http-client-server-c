override CFILES := $(shell find ./ -type f -name '*.c' -not -wholename '*example*')
override HFILES := $(shell find ./ -type f -name '*.h' -not -wholename '*example*')

.PHONY: run
run: a.out
	@./a.out

a.out: $(CFILES) $(HFILES) Makefile
#	@gcc -Wall -fsanitize=address,pointer-compare,leak,undefined,pointer-overflow -fanalyzer -ggdb $(CFILES)
	@tcc -Wall -rdynamic -fno-inline -g -bt25 -ggdb $(CFILES)