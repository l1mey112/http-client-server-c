.PHONY: run
run: a.out
	@./a.out

a.out: server.c Makefile
	@gcc -lsodium server.c