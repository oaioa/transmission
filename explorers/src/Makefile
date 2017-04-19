#!/bin/bash

CC=gcc
CFLAGS= #-Wall
all: first second third

first: server1.c useful.h
	$(CC) server1.c -o ../bin/server1-explorers $(CFLAGS) -lm 
second: server2.c useful.h
	$(CC) server2.c -o ../bin/server2-explorers $(CFLAGS) -lm 
third: server3.c useful.h
	$(CC) server3.c -o ../bin/server3-explorers $(CFLAGS) -lm 
	@echo Compilation terminée.
clean:
	rm -f ../bin/server*
	rm -rf copy_*
