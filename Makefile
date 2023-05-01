CC = gcc
CFLAGS = -Wall -Werror --std=c99

mfs: mfs.c
	$(CC) $(CFLAGS) mfs.c -o mfs

clean:
	rm -f mfs a.out