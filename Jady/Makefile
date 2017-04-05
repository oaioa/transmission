CC=gcc
CFLAGS= #-Wall
all: server client

server: server.c useful.h
	$(CC) server.c -o server $(CFLAGS)
	
client: client.c useful.h
	$(CC) client.c -o client $(CFLAGS)
	
clean:
	rm -f client server 
	rm -rf *.o 
