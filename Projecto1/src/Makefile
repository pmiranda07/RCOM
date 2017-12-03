CC=gcc
CFLAGS=-Wall -I.

nserial: main.c dataLink_layer.o app_layer.o
	$(CC) main.c dataLink_layer.o app_layer.o -o nserial $(CFLAGS)

clean:
	rm -f nserial *.o
