CC=gcc
CFLAGS=-g -std=c99

keygen: keygen.c
	$(CC) $(CFLAGS) -o keygen keygen.c

clean:
	rm -rf *.o keygen
