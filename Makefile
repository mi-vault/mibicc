CFLAGS=-std=c99 -Wall -Wextra -pedantic -g

mibicc: src/main.c
	$(CC) $(CFLAGS) -o mibicc src/main.c

test: mibicc
	./test.sh

clean:
	rm -f mibicc *.o *~ tmp*

.PHONY: test clean
