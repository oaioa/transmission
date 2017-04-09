CC=gcc
CFLAGS= #-Wall
all: server client

server: server.c useful.h
	$(CC) server.c -o server $(CFLAGS) -lm 
	
client: client.c useful.h
	$(CC) client.c -o client $(CFLAGS) -lm
	
clean:
	rm -f client server 
	rm -rf *.o
	rm recv
