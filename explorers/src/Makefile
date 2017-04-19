#!/bin/bash

CC=gcc
CFLAGS= #-Wall
all: server1-explorers

server1-explorers: server.c useful.h
	$(CC) server.c -o ../bin/server1-explorers $(CFLAGS) -lm 
	@echo Compilation terminée.
clean:
	rm -f ../bin/server*
	rm -rf copy_*
