override CFILES := $(shell find ./ -type f -name '*.c' -not -wholename '*example*')
override HFILES := $(shell find ./ -type f -name '*.h' -not -wholename '*example*')

.PHONY: run
run: a.out
	@./a.out

a.out: $(CFILES) $(HFILES) Makefile
	@gcc -ggdb $(CFILES)