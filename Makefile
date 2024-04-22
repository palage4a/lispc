ybuild:
	gcc -std=c99 -Wall -ledit main.c mpc/mpc.c -o lispc

run: build
	./a.out

