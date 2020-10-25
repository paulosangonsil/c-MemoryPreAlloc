# Makefile for prealloc

# If your compiler does not support this flags, just remove them.
# They only ensure that no new warning regressions make it into the source.
CFLAGS = -Wall -Wwrite-strings

prealloc: main.o
	$(CC) $(LFLAGS) main.o -o prealloc

main.o: main.c

clean:
	rm -f main *.o
