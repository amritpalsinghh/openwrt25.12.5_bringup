all: helloworld

helloworld: helloworld.o
	$(CC) $(LDFLAGS) helloworld.o -o helloworld

helloworld.o: helloworld.c
	$(CC) $(CFLAGS) -c -o helloworld.o helloworld.c

clean:
	rm -f helloworld helloworld.o

.PHONY: all clean
