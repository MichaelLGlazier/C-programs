CC=gcc
CFLAGS=-ansi -pedantic-errors -Wall

default: all	
	$(CC) -o pack pack.o
	$(CC) -o upack upack.o

all:		pack.o upack.o

pack.o:	pack.c
	$(CC) $(CFLAGS) -c pack.c

upack.o:	upack.c
	$(CC) $(CFLAGS) -c upack.c

.PHONY: clean

clean:
	rm -rf pack upack pack.o upack.o