CC=gcc
FLAGS = -W -std=c99 -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600 -O2

.PHONY: build

setup: 
	mkdir -p .bin

build: setup
	$(CC) $(FLAGS) main.c -o ./.bin/rld -lrld
