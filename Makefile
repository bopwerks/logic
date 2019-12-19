SHELL = /bin/sh
CFLAGS = -pedantic -Wall -DNDEBUG

logic: main.o
	$(CC) -o $@ $^

main.o: main.c

.PHONY: clean
clean:
	rm -rf logic main.o
